#include "buf_connect_server/session/session_manager.hpp"
#include "buf_connect_server/auth/user_store.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

// Generated protobuf header — produced by cmake codegen from
// buf_connect_server.proto.
#include "buf_connect_server.pb.h"


std::string GenerateUuid() {
    std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    auto v1 = dist(rng), v2 = dist(rng);
    oss << std::hex << std::setfill('0')
        << std::setw(8) << (v1 >> 32) << "-"
        << std::setw(4) << ((v1 >> 16) & 0xFFFF) << "-"
        << std::setw(4) << (v1 & 0xFFFF) << "-"
        << std::setw(4) << (v2 >> 48) << "-"
        << std::setw(12) << (v2 & 0xFFFFFFFFFFFFull);
    return oss.str();
}

namespace buf_connect_server::session {

// ---------------------------------------------------------------------------
// Static
// ---------------------------------------------------------------------------
    v2::SessionMode to_v2_session_mode(const SessionMode & _)
    {
        switch (_) {
            case SessionMode::ActiveAdmin:  return v2::SessionMode::SESSION_MODE_ACTIVE_ADMIN;
            case SessionMode::Active:       return v2::SessionMode::SESSION_MODE_ACTIVE;
            case SessionMode::Observer:     return v2::SessionMode::SESSION_MODE_OBSERVER;
            case SessionMode::OnService:    return v2::SessionMode::SESSION_MODE_ON_SERVICE;
            default:                        return v2::SessionMode::SESSION_MODE_UNSPECIFIED;
        }
    }

    v2::UserRole to_v2_user_role(const UserRole & _)
    {
        switch (_) {
            case UserRole::Admin:       return v2::UserRole::USER_ROLE_ADMIN;
            case UserRole::Engineer:    return v2::UserRole::USER_ROLE_ENGINEER;
            default:                    return v2::UserRole::USER_ROLE_UNSPECIFIED;
        }
    }

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

    struct SessionManager::Impl {
        mutable std::mutex mutex_;

        // Primary map: session_uuid → SessionEntry
        std::unordered_map<SessionUuid, SessionEntry> sessions_;

        // Subscriber map: session_uuid → EventCallback (same callback, kept separate
        // so Broadcast doesn't need to iterate all sessions to find active ones).
        std::unordered_map<SessionUuid, EventCallback> subscribers_;

        // user_id → session_uuid for the currently-live session per user.
        // A "live" session is one that has passed through Connect() and is still
        // active (has not been Disconnect()ed or TakeOver()n).
        std::unordered_map<uint64_t , SessionUuid> user_session_index_;

        // session_uuids that have been force-displaced by TakeOver.
        // Any WatchSessionEvents call presenting one of these must be rejected.
        std::unordered_map<SessionUuid, TimePoint> invalidated_sessions_;

        // Expiry loop
        std::thread       expiry_thread_;
        std::atomic<bool> expiry_running_{false};
        uint32_t          grace_period_admin_    = 45;
        uint32_t          grace_period_engineer_ = 90;

        // ------------------------------------------------------------------
        // Helpers
        // ------------------------------------------------------------------

        // Send an event to a single session; returns false if the callback
        // reports the stream is closed.
        bool SendEvent(const SessionUuid& _session_uuid,
                       const buf_connect_server::v2::SessionEvent& event) {
            auto it = subscribers_.find(_session_uuid);
            if (it == subscribers_.end()) return false;
            return it->second(event);
        }

        // Build a ForcedLogoutEvent with the given reason.
        static buf_connect_server::v2::SessionEvent MakeForcedLogout(const std::string& reason) {
            buf_connect_server::v2::SessionEvent ev;
            ev.mutable_forced_logout()->set_reason(reason);
            return ev;
        }

        // Build a ModeChangedEvent.
        static buf_connect_server::v2::SessionEvent MakeModeChanged(SessionMode mode) {
            buf_connect_server::v2::SessionEvent ev;
            ev.mutable_mode_changed()->set_new_mode(
                    static_cast<buf_connect_server::v2::SessionMode>(mode));
            return ev;
        }

        // Build a SessionStartedEvent.
        static buf_connect_server::v2::SessionEvent MakeSessionStarted(const std::string& session_id, SessionMode mode, UserRole role)
        {
            buf_connect_server::v2::SessionEvent ev;
            auto* ss = ev.mutable_session_started();
            ss->set_session_id(session_id);
            ss->set_session_mode(static_cast<buf_connect_server::v2::SessionMode>(mode));
            ss->set_role(static_cast<buf_connect_server::v2::UserRole>(role));
            return ev;
        }

        // Build an AdminConflictEvent.
        static buf_connect_server::v2::SessionEvent MakeAdminConflict(const std::string& challenger_session_id)
        {
            buf_connect_server::v2::SessionEvent ev;
            ev.mutable_admin_conflict()->set_challenger_session_id(challenger_session_id);
            return ev;
        }

        // Find the session_uuid for the currently-active admin, or return "".
        SessionUuid FindActiveAdminSession() const {
            for (const auto& [sid, entry] : sessions_) {
                if (entry.mode == SessionMode::ActiveAdmin) return sid;
            }
            return {};
        }

        // Find the session_uuid for the currently-active engineer (ACTIVE mode), or "".
        SessionUuid FindActiveEngineerSession() const {
            for (const auto& [sid, entry] : sessions_) {
                if (entry.mode == SessionMode::Active) return sid;
            }
            return {};
        }

        // Determine initial mode for an incoming session, under the lock.
        SessionMode AssignMode(UserRole role, const std::string& session_id) {
            if (role == UserRole::Admin) {
                std::string existing_admin = FindActiveAdminSession();
                if (existing_admin.empty()) {
                    // No active admin — become ActiveAdmin and notify all engineers.
                    return SessionMode::ActiveAdmin;
                } else {
                    // Active admin already present — fire conflict event at the incumbent.
                    SendEvent(existing_admin, MakeAdminConflict(session_id));
                    return SessionMode::Observer;
                }
            } else {
                // Engineer
                std::string active_admin = FindActiveAdminSession();
                if (!active_admin.empty()) {
                    return SessionMode::OnService;
                }
                std::string active_eng = FindActiveEngineerSession();
                if (active_eng.empty()) {
                    return SessionMode::Active;
                }
                return SessionMode::Observer;
            }
        }

        // When a new admin joins as ActiveAdmin, broadcast OnService to all engineers.
        void BroadcastOnServiceToEngineers() {
            auto ev = MakeModeChanged(SessionMode::OnService);
            for (auto& [sid, entry] : sessions_) {
                if (entry.role == UserRole::Engineer &&
                    (entry.mode == SessionMode::Active || entry.mode == SessionMode::Observer)) {
                    entry.mode = SessionMode::OnService;
                    SendEvent(sid, ev);
                }
            }
        }

        // When the active admin disconnects, restore engineers to Active/Observer.
        void RestoreEngineersOnAdminLeave() {
            bool first = true;
            auto active_ev   = MakeModeChanged(SessionMode::Active);
            auto observer_ev = MakeModeChanged(SessionMode::Observer);
            for (auto& [sid, entry] : sessions_) {
                if (entry.role == UserRole::Engineer &&
                    entry.mode == SessionMode::OnService) {
                    if (first) {
                        entry.mode = SessionMode::Active;
                        SendEvent(sid, active_ev);
                        first = false;
                    } else {
                        entry.mode = SessionMode::Observer;
                        SendEvent(sid, observer_ev);
                    }
                }
            }
        }

        // Erase a session from all internal maps thread safe
        void EraseSession(const SessionUuid & _session_uuid) {
            std::lock_guard<std::mutex> lock(mutex_);
            sessions_.erase(_session_uuid);
            subscribers_.erase(_session_uuid);
            auto size_before = user_session_index_.size();
            auto it = std::find_if(user_session_index_.begin(), user_session_index_.end(),
                         [&_session_uuid](auto _item) { return (_item.second == _session_uuid); });
            if (it == user_session_index_.end()) {
                SPDLOG_ERROR("Did not found user with that session_uuid");
            }
            user_session_index_.erase(it);
            if (size_before == user_session_index_.size()) {
                SPDLOG_ERROR("Did not succeed to delete user");
            }
        }
    };

// ---------------------------------------------------------------------------
// SessionManager — constructor / destructor
// ---------------------------------------------------------------------------

    SessionManager::SessionManager() : impl_(std::make_unique<Impl>()) {}
    SessionManager::~SessionManager() { StopExpiryLoop(); }

// ---------------------------------------------------------------------------
// Connect
// ---------------------------------------------------------------------------

    SessionEntry SessionManager::CreateSession(uint64_t user_id, const std::string & _user_role, EventCallback _callback)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        auto new_session_uuid = GenerateUuid();
        SessionEntry entry;
        // ------------------------------------------------------------------
        // 1.  Check whether this session_uuid was invalidated by a TakeOver.
        // ------------------------------------------------------------------
        if (impl_->invalidated_sessions_.count(new_session_uuid)) {
            spdlog::warn("session_manager: CreateSession rejected — session '{}' is invalidated "
                         "(taken over)", new_session_uuid);
            // Send the forced-logout event so the client can react, then refuse.
            auto ev = Impl::MakeForcedLogout("taken_over");
            _callback(ev);
            entry.session_uuid = "";
            entry.mode = SessionMode::Unspecified;
            entry.role = UserRole::Unspecified;
            entry.user_id = 0;
            return entry; // stream should be closed by the caller
        }

        // ------------------------------------------------------------------
        // 2.  Build the session entry.
        // ------------------------------------------------------------------
        auto now = Clock::now();

        entry.session_uuid   = new_session_uuid;
        entry.user_id        = user_id;
        entry.role           = (_user_role == "admin") ? UserRole::Admin : UserRole::Engineer;
        entry.started_at     = now;
        entry.last_heartbeat = now;
        entry.callback       = _callback;

        // ------------------------------------------------------------------
        // 3.  Determine mode.
        // ------------------------------------------------------------------
        SessionMode mode = impl_->AssignMode(entry.role, new_session_uuid);
        entry.mode = mode;

        // ------------------------------------------------------------------
        // 4.  Register the session.
        // ------------------------------------------------------------------
        impl_->sessions_[new_session_uuid]    = std::move(entry);
        impl_->subscribers_[new_session_uuid] = _callback;

        // ------------------------------------------------------------------
        // 5.  Update user_session_index_.
        // ------------------------------------------------------------------
        impl_->user_session_index_[user_id] = new_session_uuid;

        // ------------------------------------------------------------------
        // 6.  Side effects of mode assignment.
        // ------------------------------------------------------------------
        if (mode == SessionMode::ActiveAdmin) {
            impl_->BroadcastOnServiceToEngineers();
        }

        // ------------------------------------------------------------------
        // 7.  Send SessionStartedEvent to this session.
        // ------------------------------------------------------------------
        auto started_ev = Impl::MakeSessionStarted(new_session_uuid, mode, entry.role);
        _callback(started_ev);

        std::string mode_str;
        switch (mode) {
            case SessionMode::ActiveAdmin: { mode_str = "active_admin"; break; }
            case SessionMode::Active: { mode_str = "active_engineer"; break; }
            case SessionMode::Observer: { mode_str = "observer"; break; }
            case SessionMode::OnService: { mode_str = "on_service"; break; }
            default: mode_str = "unspecified";
        }

        spdlog::info("session_manager: CreateSession session_uuid='{}' user='{}' role={} mode={}",
                     new_session_uuid, user_id, _user_role, mode_str);

        return entry;
    }

// ---------------------------------------------------------------------------
// Disconnect
// ---------------------------------------------------------------------------

    void SessionManager::Disconnect(const std::string& session_uuid, uint64_t user_id) {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        auto it = impl_->sessions_.find(session_uuid);
        if (it == impl_->sessions_.end()) {
            spdlog::debug("session_manager: Disconnect called for unknown session '{}'", session_uuid);
            return;
        }

        auto mode = it->second.mode;
        std::string role_str = it->second.role == UserRole::Admin ? "admin " : "engineer";
        impl_->EraseSession(session_uuid);

        // Update user_session_index_ only if this connection_id is still the
        // registered one (a TakeOver may have already replaced it).
        {
            auto idx_it = impl_->user_session_index_.find(user_id);
            if (idx_it != impl_->user_session_index_.end() &&
                idx_it->second == session_uuid) {
                impl_->user_session_index_.erase(idx_it);
            }
        }

        spdlog::info("session_manager: Disconnect session='{}' user='{}' role='{}' mode={}",
                     session_uuid, user_id, role_str, static_cast<int>(mode));

        // If the departing session was the active admin, restore engineers.
        if (mode == SessionMode::ActiveAdmin) {
            impl_->RestoreEngineersOnAdminLeave();
        }
    }

// ---------------------------------------------------------------------------
// UpdateHeartbeat / Touch
// ---------------------------------------------------------------------------

    void SessionManager::UpdateHeartbeat(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        auto it = impl_->sessions_.find(session_id);
        if (it != impl_->sessions_.end()) {
            it->second.last_heartbeat = Clock::now();
        }
    }

    void SessionManager::Touch(const std::string& session_id) {
        UpdateHeartbeat(session_id);
    }

// ---------------------------------------------------------------------------
// HasLiveSession
// ---------------------------------------------------------------------------

    bool SessionManager::HasLiveSession(uint64_t user_id) const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->user_session_index_.count(user_id) > 0;
    }

// ---------------------------------------------------------------------------
// GetLiveSessionInfo
// ---------------------------------------------------------------------------

    LiveSessionInfo SessionManager::GetLiveSessionInfo(uint64_t user_id) const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        auto idx_it = impl_->user_session_index_.find(user_id);
        if (idx_it == impl_->user_session_index_.end()) {
            spdlog::error("session_manager: GetLiveSessionInfo called for user '{}' "
                          "who has no live session", user_id);
            return {};
        }

        const std::string& session_uuid = idx_it->second;

        auto entry = impl_->sessions_.find(session_uuid);
        if (entry == impl_->sessions_.end()) { // Should not happen if user_session_index_ is kept consistent.
            SPDLOG_ERROR("session_manager: GetLiveSessionInfo found index entry for user '{}' "
                          "but no matching session", user_id);
            return {};
        }

        LiveSessionInfo info;
        info.session_uuid = session_uuid;
        info.role          = (entry->second.role == UserRole::Admin) ? "admin" : "engineer";
        info.started_at    = entry->second.started_at;
        return info;
    }

// ---------------------------------------------------------------------------
// TakeOver
// ---------------------------------------------------------------------------

    v2::SessionMode SessionManager::TakeOver(uint64_t _user_id, const SessionUuid & _session_uuid_taking_over)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        auto idx_it = impl_->user_session_index_.find(_user_id);
        if (idx_it == impl_->user_session_index_.end()) {
            spdlog::warn("session_manager: TakeOver — no live session for user '{}'", _user_id);
            return v2::SessionMode::SESSION_MODE_UNSPECIFIED;
        }

        const std::string& old_session_uuid = idx_it->second;

        // 1. Send forced-logout to the old session.
        auto ev = Impl::MakeForcedLogout("taken_over");
        impl_->SendEvent(old_session_uuid, ev);
        auto session = impl_->sessions_.find(old_session_uuid);
        v2::SessionMode session_mode;
        switch (session->second.mode) {
            case SessionMode::ActiveAdmin: { session_mode = v2::SessionMode::SESSION_MODE_ACTIVE_ADMIN; break; }
            case SessionMode::Active: { session_mode = v2::SessionMode::SESSION_MODE_ACTIVE; break; }
            case SessionMode::Observer: { session_mode = v2::SessionMode::SESSION_MODE_OBSERVER; break; }
            case SessionMode::OnService: { session_mode = v2::SessionMode::SESSION_MODE_ON_SERVICE; break; }
            default: session_mode = v2::SessionMode::SESSION_MODE_UNSPECIFIED;
        }
        // 2. Erase the old session from the state machine.
        impl_->EraseSession(old_session_uuid);

        // 4. Mark old session_id as invalidated so any concurrent WatchSessionEvents
        //    call for it is rejected immediately.
//        impl_->invalidated_sessions_[old_session_id] = Clock::now();

        // Note: new_session_id is NOT added to invalidated_sessions_ — it is the
        // replacement and must be allowed through Connect().
        (void)_session_uuid_taking_over;
        return session_mode;
    }

// ---------------------------------------------------------------------------
// IsSessionInvalidated
// ---------------------------------------------------------------------------

    bool SessionManager::IsSessionInvalidated(const std::string& session_id) const {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->invalidated_sessions_.count(session_id) > 0;
    }

// ---------------------------------------------------------------------------
// ClaimActiveRole
// ---------------------------------------------------------------------------

    SessionMode SessionManager::ClaimActiveRole(const SessionUuid & _session_uuid)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        auto it = impl_->sessions_.find(_session_uuid);
        if (it == impl_->sessions_.end()) {
            spdlog::warn("session_manager: ClaimActiveRole — unknown session '{}'", _session_uuid);
            return SessionMode::Observer;
        }

        SessionEntry& entry = it->second;

        if (entry.mode != SessionMode::Observer) {
            // Already in an active mode; nothing to do.
            return entry.mode;
        }

        // Try to promote.
        if (entry.role == UserRole::Engineer) {
            if (impl_->FindActiveAdminSession().empty() &&
                impl_->FindActiveEngineerSession().empty()) {
                entry.mode = SessionMode::Active;
                auto ev = Impl::MakeModeChanged(SessionMode::Active);
                impl_->subscribers_[_session_uuid](ev);
                spdlog::info("session_manager: ClaimActiveRole session='{}' → ACTIVE",
                             _session_uuid);
            }
        } else if (entry.role == UserRole::Admin) {
            std::string existing_admin = impl_->FindActiveAdminSession();
            if (existing_admin.empty()) {
                entry.mode = SessionMode::ActiveAdmin;
                auto ev = Impl::MakeModeChanged(SessionMode::ActiveAdmin);
                impl_->subscribers_[_session_uuid](ev);
                impl_->BroadcastOnServiceToEngineers();
                spdlog::info("session_manager: ClaimActiveRole session='{}' → ACTIVE_ADMIN",
                             _session_uuid);
            }
        }

        return entry.mode;
    }

// ---------------------------------------------------------------------------
// HandleAdminConflictChoice
// ---------------------------------------------------------------------------

    void SessionManager::HandleAdminConflictChoice(const SessionUuid & _incumbent_session_uuid, int choice_value)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        // session_id is the incumbent admin's session.
        auto it = impl_->sessions_.find(_incumbent_session_uuid);
        if (it == impl_->sessions_.end()) return;

        // choice_value mirrors proto AdminConflictChoice enum:
        //   1 = GRANT, 2 = KEEP, 3 = SNOOZE
        enum { GRANT = 1, KEEP = 2, SNOOZE = 3 };

        if (choice_value == GRANT) {
            // Force-logout the incumbent; the challenger should call ClaimActiveRole.
            auto logout_ev = Impl::MakeForcedLogout("taken_over_by_another_");
            impl_->SendEvent(_incumbent_session_uuid, logout_ev);
            impl_->EraseSession(_incumbent_session_uuid);
            spdlog::info("session_manager: AdminConflict GRANT — incumbent '{}' evicted",
                         _incumbent_session_uuid);
        } else if (choice_value == KEEP) {
            // Challenger stays as Observer - no action
            spdlog::info("session_manager: AdminConflict KEEP — incumbent '{}' retained",
                         _incumbent_session_uuid);
        } else if (choice_value == SNOOZE) {
            // No immediate action; conflict can be re-raised.
            spdlog::info("session_manager: AdminConflict SNOOZE — conflict deferred for '{}'",
                         _incumbent_session_uuid);
        }
    }

// ---------------------------------------------------------------------------
// Broadcast
// ---------------------------------------------------------------------------

    void SessionManager::Broadcast(const buf_connect_server::v2::SessionEvent& event)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        std::vector<SessionUuid> dead;
        for (auto& [sid, cb] : impl_->subscribers_) {
            if (!cb(event))
                dead.push_back(sid);
        }
        for (const auto& sid : dead) {
            // Stream closed; clean up gracefully (without user_id context here,
            // we only erase from subscribers_ and sessions_; user_session_index_
            // will be cleaned up on the next Disconnect call from the handler).
            spdlog::debug("session_manager: Broadcast — stream closed for session '{}'", sid);
            impl_->sessions_.erase(sid);
            impl_->subscribers_.erase(sid);
        }
    }

// ---------------------------------------------------------------------------
// StartExpiryLoop
// ---------------------------------------------------------------------------

    void SessionManager::StartExpiryLoop(uint32_t grace_admin_seconds,
                                         uint32_t grace_engineer_seconds)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        impl_->grace_period_admin_    = grace_admin_seconds;
        impl_->grace_period_engineer_ = grace_engineer_seconds;

        impl_->expiry_running_.store(true);

        impl_->expiry_thread_ = std::thread([this] {
            spdlog::info("session_manager: expiry loop started "
                         "(grace_admin={}s grace_engineer={}s)",
                         impl_->grace_period_admin_,
                         impl_->grace_period_engineer_);

            while (impl_->expiry_running_.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                if (!impl_->expiry_running_.load()) break;

                std::vector<std::pair<SessionUuid , uint64_t >> to_evict; // {session_id, user_id}

                {
                    std::lock_guard<std::mutex> lock(impl_->mutex_);
                    auto now = Clock::now();

                    for (const auto& [session_uuid, entry] : impl_->sessions_) {
                        // Select grace period by role.
                        uint32_t grace = (entry.role == UserRole::Admin)
                                         ? impl_->grace_period_admin_
                                         : impl_->grace_period_engineer_;

                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                now - entry.last_heartbeat).count();

                        if (elapsed > static_cast<long long>(grace)) {
                            spdlog::info("session_manager: expiring session='{}' user='{}' "
                                         "elapsed={}s grace={}s",
                                         session_uuid, entry.user_id, elapsed, grace);
                            to_evict.emplace_back(session_uuid, entry.user_id);
                        }
                    }

                    // Evict within the same lock to avoid races.
                    for (const auto& [session_uuid, uid] : to_evict) {
                        auto sess_it = impl_->sessions_.find(session_uuid);
                        if (sess_it != impl_->sessions_.end()) {
                            auto ev = Impl::MakeForcedLogout("session_expired");
                            impl_->SendEvent(session_uuid, ev);
                            // Update maps
                            impl_->EraseSession(session_uuid);
                            // Handle admin departure side effects.
                            if (sess_it->second.mode == SessionMode::ActiveAdmin) {
                                impl_->RestoreEngineersOnAdminLeave();
                                continue;
                            }
                        }
                    }
                    to_evict.clear();
                    // Purge old invalidated_sessions_ entries (older than 10 minutes)
                    // to prevent unbounded growth.
                    auto cutoff = now - std::chrono::minutes(10);
                    for (auto it = impl_->invalidated_sessions_.begin();
                         it != impl_->invalidated_sessions_.end(); ) {
                        if (it->second < cutoff) {
                            it = impl_->invalidated_sessions_.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }

            spdlog::info("session_manager: expiry loop stopped");
        });
    }

// ---------------------------------------------------------------------------
// StopExpiryLoop
// ---------------------------------------------------------------------------

    void SessionManager::StopExpiryLoop() {
        impl_->expiry_running_.store(false);
        if (impl_->expiry_thread_.joinable()) {
            impl_->expiry_thread_.join();
        }
    }

} // namespace buf_connect_server::session
