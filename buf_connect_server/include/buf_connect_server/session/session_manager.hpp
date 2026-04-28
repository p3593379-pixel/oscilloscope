#ifndef BUF_CONNECT_SERVER_SESSION_SESSION_MANAGER_HPP
#define BUF_CONNECT_SERVER_SESSION_SESSION_MANAGER_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include "buf_connect_server.pb.h"

// Forward-declared protobuf type used in the event callback signature.
namespace buf_connect_server::v2 { class SessionEvent; }

namespace buf_connect_server::session {

// ---------------------------------------------------------------------------
// SessionMode / UserRole mirrors
// ---------------------------------------------------------------------------
// Lightweight C++ enums that shadow the proto enums.  Using these in the
// SessionManager API avoids a hard dependency on protobuf headers in
// every translation unit that includes session_manager.hpp.

    enum class SessionMode {
        Unspecified  = 0,
        Active       = 1,  // active engineer
        ActiveAdmin  = 2,  // active admin
        Observer     = 3,  // read-only (conflict or second session)
        OnService    = 4,  // engineer in read-only because admin is active
    };

    enum class UserRole {
        Admin,
        Engineer,
        Unspecified
    };

    using SessionUuid = std::string;

    v2::SessionMode to_v2_session_mode(const SessionMode & _);
    v2::UserRole to_v2_user_role(const UserRole & _);

    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
// ---------------------------------------------------------------------------
// EventCallback
// ---------------------------------------------------------------------------
// Callback type used to push SessionEvent messages down a WatchSessionEvents
// stream.  Returns false if the stream has been closed by the client.

    using EventCallback = std::function<bool(const buf_connect_server::v2::SessionEvent&)>;

    struct SessionEntry {
        std::string  session_uuid;
        uint64_t  user_id {0};
        UserRole     role          = UserRole::Unspecified;
        SessionMode  mode          = SessionMode::Unspecified;
        TimePoint    started_at;
        TimePoint    last_heartbeat;
        EventCallback callback;
    };

// ---------------------------------------------------------------------------
// LiveSessionInfo
// ---------------------------------------------------------------------------
// Returned by SessionManager::GetLiveSessionInfo().  Describes the currently
// active (live) session for a given user_id.

    struct LiveSessionInfo {
        /// session_uuid assigned to the session when it entered SessionManager.
        std::string session_uuid;

        /// Role of the session owner ("engineer" or "admin").
        std::string role;

        /// When the session was established (steady_clock, for grace-period math).
        std::chrono::steady_clock::time_point started_at;
    };


// ---------------------------------------------------------------------------
// SessionManager
// ---------------------------------------------------------------------------
// Thread-safe session state machine.  All public methods are safe to call
// from any thread.

    class SessionManager {
    public:
        SessionManager();
        ~SessionManager();

        // Non-copyable, non-movable (internal mutex).
        SessionManager(const SessionManager&)            = delete;
        SessionManager& operator=(const SessionManager&) = delete;

        // -----------------------------------------------------------------------
        // Lifecycle
        // -----------------------------------------------------------------------

        /// CreateSession a new session.  Assigns a SessionMode and begins pushing events
        /// to `callback`.  Returns the assigned mode (can be Observer even for the
        /// first connection of a role if TakeOver invalidated the session).
        ///
        /// On success: adds entry to user_session_index_[user_id] = connection_id.
        /// If session_id is in invalidated_sessions_: sends ForcedLogoutEvent and
        /// returns SessionMode::Observer (the caller should close the stream).
        SessionEntry CreateSession(uint64_t user_id, const std::string & _user_role, EventCallback _callback);

        /// Disconnect a session.  Removes it from all internal maps.
        /// If user_session_index_[user_id] == connection_id, removes that entry too.
        void Disconnect(const std::string& session_uuid,
                        uint64_t user_id);

        /// Update the last-heartbeat timestamp for the given session.
        void UpdateHeartbeat(const std::string& session_id);

        // -----------------------------------------------------------------------
        // Session invalidation / takeover
        // -----------------------------------------------------------------------

        /// Returns true if `user_id` has a live session registered in
        /// user_session_index_.
        bool HasLiveSession(uint64_t user_id) const;

        /// Returns the LiveSessionInfo for the user's active session.
        /// Precondition: HasLiveSession(user_id) == true.
        LiveSessionInfo GetLiveSessionInfo(uint64_t user_id) const;

        /// Force-displace the existing session for `user_id`.
        ///
        /// Steps:
        ///  1. Looks up the old connection_id from user_session_index_.
        ///  2. Sends ForcedLogoutEvent{"taken_over"} to the old session's callback.
        ///  3. Erases the old session from sessions_ and subscribers_.
        ///  4. Removes user_id from user_session_index_.
        ///  5. Adds the old session_id to invalidated_sessions_ so any in-flight
        ///     WatchSessionEvents call for that session is rejected immediately.
        ///
        /// `new_session_id` is the session_id that will replace the old one;
        /// it is stored here so that, when the new WatchSessionEvents arrives,
        /// CreateSession() can recognise it is NOT invalidated.
        v2::SessionMode TakeOver(uint64_t _user_id,
                      const SessionUuid &_session_uuid_taking_over);

        /// Returns true if `session_id` has been evicted via TakeOver and any
        /// attempt to use it should be rejected.
        bool IsSessionInvalidated(const std::string& session_id) const;

        /// Alias for UpdateHeartbeat — public, used by HandleRenewCallToken as the
        /// control-plane heartbeat signal.
        void Touch(const std::string& session_id);

        // -----------------------------------------------------------------------
        // Role claims and admin conflict
        // -----------------------------------------------------------------------

        /// Attempt to promote an Observer to an active role.
        /// Returns the new SessionMode (may still be Observer if conditions are not met).
        SessionMode ClaimActiveRole(const SessionUuid &_session_uuid);

        /// Broadcast the admin's response to a conflict challenge.
        void HandleAdminConflictChoice(const SessionUuid &_incumbent_session_uuid,
                                       int                choice_value);

        // -----------------------------------------------------------------------
        // Stream token broadcast (data plane)
        // -----------------------------------------------------------------------

        /// Broadcast a generic SessionEvent to all connected sessions.
        void Broadcast(const buf_connect_server::v2::SessionEvent& event);

        // -----------------------------------------------------------------------
        // Expiry loop
        // -----------------------------------------------------------------------

        /// Start the background thread that periodically evicts sessions that have
        /// not sent a heartbeat within their role-specific grace period.
        ///
        ///   grace_admin_seconds    — grace period for ADMIN sessions.
        ///   grace_engineer_seconds — grace period for ENGINEER (and ACTIVE) sessions.
        ///
        /// Grace period is measured from the session's last_heartbeat timestamp;
        /// it is independent of the token TTL.
        void StartExpiryLoop(uint32_t grace_admin_seconds,
                             uint32_t grace_engineer_seconds);

        /// Stop the expiry loop (called on server shutdown).
        void StopExpiryLoop();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace buf_connect_server::session

#endif  // BUF_CONNECT_SERVER_SESSION_SESSION_MANAGER_HPP
