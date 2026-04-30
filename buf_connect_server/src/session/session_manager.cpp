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

        // user_uuid → session_uuid for the currently-live session per user.
        // A "live" session is one that has passed through Connect() and is still
        // active (has not been Disconnect()ed or TakeOver()n).
        std::unordered_map<UserUuid, SessionUuid> user_session_index_;

        // session_uuids that have been force-displaced by TakeOver.
        // Any WatchSessionEvents call presenting one of these must be rejected.
        std::unordered_map<SessionUuid, TimePoint> invalidated_sessions_;

        // Expiry loop
        std::thread       expiry_thread_;
        std::atomic<bool> expiry_running_{false};

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

        void RegisterSession(SessionEntry & _session_entry)
        {
            SessionMode mode;
            if (_session_entry.role == UserRole::Admin) {
                std::string existing_admin = FindActiveAdminSession();
                if (existing_admin.empty()) {
                    // No active admin — become ActiveAdmin and notify all engineers.
                    BroadcastOnServiceToEngineers();
                    mode = SessionMode::ActiveAdmin;
                } else {
                    // Active admin already present — fire conflict event at the incumbent.
                    SendEvent(existing_admin, MakeAdminConflict(_session_entry.session_uuid));
                    mode = SessionMode::Observer;
                }
            } else {
                // Engineer
                std::string active_admin = FindActiveAdminSession();
                if (!active_admin.empty()) {
                    mode = SessionMode::OnService;
                }
                std::string active_eng = FindActiveEngineerSession();
                if (active_eng.empty()) {
                    mode = SessionMode::Active;
                }
            }
            _session_entry.mode = mode;
            sessions_[_session_entry.session_uuid] = _session_entry;
            subscribers_[_session_entry.session_uuid] = _session_entry.callback;
            user_session_index_[_session_entry.user_id] = _session_entry.session_uuid;
            auto started_ev = MakeSessionStarted(_session_entry.session_uuid, _session_entry.mode, _session_entry.role);
            _session_entry.callback(started_ev);
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

    SessionEntry SessionManager::BuildNewSession(const UserUuid & _user_uuid, const std::string & _user_role, EventCallback _event_callback)
    {
        auto new_session_uuid = GenerateUuid();
        SessionEntry entry;
        auto now = Clock::now();

        entry.session_uuid   = new_session_uuid;
        entry.user_id        = _user_uuid;
        entry.role           = (_user_role == "admin") ? UserRole::Admin : UserRole::Engineer;
        entry.mode           = SessionMode::Unspecified; // Not specified without context
        entry.started_at     = now;
        entry.last_heartbeat = now;
        entry.callback       = _event_callback;
        return entry;
    }

    void SessionManager::RegisterSession(SessionEntry & _session_entry)
    {
        std::lock_guard<std::mutex> _(impl_->mutex_);
        impl_->RegisterSession(_session_entry);
    }

// ---------------------------------------------------------------------------
// UpdateHeartbeat / Touch
// ---------------------------------------------------------------------------

    void SessionManager::UpdateHeartbeat(const SessionUuid & _session_uuid) {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        auto it = impl_->sessions_.find(_session_uuid);
        if (it != impl_->sessions_.end()) {
            it->second.last_heartbeat = Clock::now();
        }
    }

    void SessionManager::Touch(const SessionUuid &_session_uuid) {
        UpdateHeartbeat(_session_uuid);
    }

// ---------------------------------------------------------------------------
// HasLiveSession
// ---------------------------------------------------------------------------

    bool SessionManager::HasLiveSession(const UserUuid &_user_uuid) const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->user_session_index_.count(_user_uuid) > 0;
    }

// ---------------------------------------------------------------------------
// GetLiveSessionInfo
// ---------------------------------------------------------------------------

    LiveSessionInfo SessionManager::GetLiveSessionInfo(const UserUuid &_user_uuid) const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        auto idx_it = impl_->user_session_index_.find(_user_uuid);
        if (idx_it == impl_->user_session_index_.end()) {
            spdlog::error("session_manager: GetLiveSessionInfo called for user '{}' "
                          "who has no live session", _user_uuid);
            return {};
        }

        const std::string& session_uuid = idx_it->second;

        auto entry = impl_->sessions_.find(session_uuid);
        if (entry == impl_->sessions_.end()) { // Should not happen if user_session_index_ is kept consistent.
            SPDLOG_ERROR("session_manager: GetLiveSessionInfo found index entry for user '{}' "
                          "but no matching session", _user_uuid);
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

    v2::SessionMode SessionManager::TakeOver(const UserUuid &_user_uuid, const SessionUuid & _session_uuid_taking_over)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        auto idx_it = impl_->user_session_index_.find(_user_uuid);
        if (idx_it == impl_->user_session_index_.end()) {
            spdlog::warn("session_manager: TakeOver — no live session for user '{}'", _user_uuid);
            return v2::SessionMode::SESSION_MODE_UNSPECIFIED;
        }

        const std::string& old_session_uuid = idx_it->second;

        // 1. Send forced-logout to the old session.
        auto ev = Impl::MakeForcedLogout("taken_over");
        impl_->SendEvent(old_session_uuid, ev);
        auto session = impl_->sessions_.find(old_session_uuid);
        std::string role_str = session->second.role == UserRole::Admin ? "admin" : "engineer";
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

        // 3. Register new one, but with already known from pending list UUID
        auto new_session_entry = BuildNewSession(_user_uuid, role_str, session->second.callback);
        new_session_entry.session_uuid = _session_uuid_taking_over;
        impl_->RegisterSession(new_session_entry);

        // Note: new_session_id is NOT added to invalidated_sessions_ — it is the
        // replacement and must be allowed through Connect().
        (void)_session_uuid_taking_over;
        return session_mode;
    }

// ---------------------------------------------------------------------------
// IsSessionInvalidated
// ---------------------------------------------------------------------------

    bool SessionManager::IsSessionInvalidated(const SessionUuid & _session_uuid) const {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->sessions_.count(_session_uuid) == 0;
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

        impl_->expiry_running_.store(true);

        impl_->expiry_thread_ = std::thread([this, grace_admin_seconds, grace_engineer_seconds] {
            spdlog::info("session_manager: expiry loop started "
                         "(grace_admin={}s grace_engineer={}s)",
                         grace_admin_seconds,
                         grace_engineer_seconds);

            while (impl_->expiry_running_.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                if (!impl_->expiry_running_.load()) break;

                std::vector<std::pair<SessionUuid , UserUuid >> to_evict; // {session_id, user_id}

                {
                    std::lock_guard<std::mutex> lock(impl_->mutex_);
                    auto now = Clock::now();

                    for (const auto& [session_uuid, entry] : impl_->sessions_) {
                        // Select grace period by role.
                        uint32_t grace = (entry.role == UserRole::Admin)
                                         ? grace_admin_seconds
                                         : grace_engineer_seconds;

                        std::cout << session_uuid << ":\t";
                        auto now_c = std::chrono::system_clock::to_time_t(now);
                        std::cout << "Now" << std::put_time(std::localtime(&now_c), "%F %T") << "\t";
                        auto lhb = std::chrono::system_clock::to_time_t(entry.last_heartbeat);
                        std::cout << "Last Heartbeat" << std::put_time(std::localtime(&lhb), "%F %T") << "\t";

                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                now - entry.last_heartbeat).count();
                        std::cout << "elapsed: " << elapsed << std::endl;
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
