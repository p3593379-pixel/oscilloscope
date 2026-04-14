#include "buf_connect_server/session/session_manager.hpp"
#include "buf_connect_server/session/session_event.hpp"
#include "buf_connect_server/connect/frame_codec.hpp"
#include "buf_connect_server.pb.h"   // buf_connect_server.v2 — no oscilloscope dep
#include <spdlog/spdlog.h>
#include <mutex>
#include <unordered_map>
#include <optional>

// ─── Proto serialisation ─────────────────────────────────────────────────────

static std::vector<uint8_t> SerialiseEvent(
        const buf_connect_server::session::SessionEventVariant& ev) {
    namespace bcs = buf_connect_server::v2;
    bcs::SessionEvent proto;

    std::visit([&proto](const auto& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, buf_connect_server::session::AdminConflictEvent>) {
            auto* p = proto.mutable_admin_conflict();
            p->set_timeout_seconds(e.timeout_seconds);
            p->set_snoozeable(e.snoozeable);

        } else if constexpr (std::is_same_v<T, buf_connect_server::session::AdminPendingEvent>) {
            auto* p = proto.mutable_admin_pending();
            p->set_occupied_for_seconds(e.occupied_for_seconds);

        } else if constexpr (std::is_same_v<T, buf_connect_server::session::ConflictResolvedEvent>) {
            auto* p = proto.mutable_conflict_resolved();
            bcs::AdminConflictChoice choice = bcs::ADMIN_CONFLICT_CHOICE_KEEP;
            if (e.resolution == "GRANT")  choice = bcs::ADMIN_CONFLICT_CHOICE_GRANT;
            if (e.resolution == "SNOOZE") choice = bcs::ADMIN_CONFLICT_CHOICE_SNOOZE;
            p->set_resolution(choice);

        } else if constexpr (std::is_same_v<T, buf_connect_server::session::ForcedLogoutEvent>) {
            proto.mutable_forced_logout()->set_reason(e.reason);

        } else if constexpr (std::is_same_v<T, buf_connect_server::session::VacantRoleEvent>) {
            proto.mutable_vacant_role();

        } else if constexpr (std::is_same_v<T, buf_connect_server::session::OnServiceEvent>) {
            proto.mutable_on_service();

        } else if constexpr (std::is_same_v<T, buf_connect_server::session::ServiceEndedEvent>) {
            proto.mutable_service_ended();

        } else if constexpr (std::is_same_v<T, buf_connect_server::session::RoleClaimedEvent>) {
            proto.mutable_role_claimed()->set_user_id(e.user_id);
        }
    }, ev);

    std::vector<uint8_t> buf(proto.ByteSizeLong());
    proto.SerializeToArray(buf.data(), static_cast<int>(buf.size()));
    return buf;
}

// ─── Impl ────────────────────────────────────────────────────────────────────

class buf_connect_server::session::SessionManager::Impl {
public:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SessionState>                    sessions_;
    std::unordered_map<std::string, connect::ConnectResponseWriter*> subscribers_;
    std::optional<std::string>                                       active_admin_connection_id_;
    std::atomic<bool>  expiry_running_{false};
    std::thread        expiry_thread_;
    int64_t            access_token_ttl_{600};   // default 10 min

    void RunExpiryLoop() {
        while (expiry_running_) {
            std::this_thread::sleep_for(std::chrono::seconds(15));
            auto now = std::chrono::steady_clock::now();
            std::unique_lock lock(mutex_);
            for (auto it = sessions_.begin(); it != sessions_.end(); ) {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(
                        now - it->second.last_heartbeat).count();
                // Grace period = TTL + 30s to absorb refresh latency
                if (age > access_token_ttl_ + 30) {
                    spdlog::info("Session '{}' expired (no heartbeat for {}s)",
                                 it->first, age);
                    SendEventTo(it->first, ForcedLogoutEvent{"session_expired"});
                    subscribers_.erase(it->first);
                    if (active_admin_connection_id_ == it->first)
                        active_admin_connection_id_.reset();
                    it = sessions_.erase(it);
                } else { ++it; }
            }
        }
    }

    void SendEventTo(const std::string& connection_id,
                     const SessionEventVariant& event) {
        auto it = subscribers_.find(connection_id);
        if (it == subscribers_.end() || !it->second) return;
        auto frame = connect::EncodeFrame(
                std::span<const uint8_t>(SerialiseEvent(event)),
                connect::FrameFlag::kData);
        it->second->WriteStreamingFrame(
                std::span<const uint8_t>(SerialiseEvent(event)));
    }

    void BroadcastEvent(const SessionEventVariant& event,
                        const std::string& exclude_id = "") {
        for (auto& [cid, writer] : subscribers_) {
            if (cid == exclude_id || !writer) continue;
            writer->WriteStreamingFrame(
                    std::span<const uint8_t>(SerialiseEvent(event)));
        }
    }
};

// ─── Public API ──────────────────────────────────────────────────────────────

buf_connect_server::session::SessionManager::SessionManager()
        : impl_(std::make_unique<Impl>()) {}

buf_connect_server::session::SessionManager::~SessionManager()
{
    StopExpiryLoop();
};

buf_connect_server::session::SessionMode
buf_connect_server::session::SessionManager::Connect(
        const std::string& connection_id,
        const std::string& user_id,
        const std::string& role) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    SessionState state;
    state.connection_id = connection_id;
    state.user_id       = user_id;
    state.role          = role;
    state.last_heartbeat = std::chrono::steady_clock::now();

    if (role == "admin") {
        if (!impl_->active_admin_connection_id_) {
            state.mode = SessionMode::kActiveAdmin;
            impl_->active_admin_connection_id_ = connection_id;
            for (auto& [cid, s] : impl_->sessions_) {
                if (s.role == "engineer") {
                    s.mode = SessionMode::kOnService;
                    impl_->SendEventTo(cid, OnServiceEvent{});
                }
            }
            spdlog::info("Admin '{}' connected — ACTIVE_ADMIN", user_id);
        } else {
            state.mode = SessionMode::kObserver;
            auto existing_cid = *impl_->active_admin_connection_id_;
            impl_->SendEventTo(existing_cid, AdminConflictEvent{20, true});
            spdlog::info("Admin '{}' conflict with existing active admin", user_id);
        }
    } else {
        if (impl_->active_admin_connection_id_) {
            state.mode = SessionMode::kOnService;
        } else {
            bool has_active = false;
            for (auto& [cid, s] : impl_->sessions_) {
                if (s.role == "engineer" && s.mode == SessionMode::kActive) {
                    has_active = true; break;
                }
            }
            state.mode = has_active ? SessionMode::kObserver : SessionMode::kActive;
            if (!has_active) impl_->BroadcastEvent(VacantRoleEvent{}, connection_id);
        }
        spdlog::info("Engineer '{}' connected — {}", user_id,
                     state.mode == SessionMode::kActive    ? "ACTIVE"     :
                     state.mode == SessionMode::kOnService ? "ON_SERVICE" : "OBSERVER");
    }

    impl_->sessions_[connection_id] = state;
    return state.mode;
}

void buf_connect_server::session::SessionManager::Disconnect(
        const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto it = impl_->sessions_.find(connection_id);
    if (it == impl_->sessions_.end()) return;

    auto& state = it->second;
    if (state.role == "admin" &&
        impl_->active_admin_connection_id_ == connection_id) {
        impl_->active_admin_connection_id_.reset();
        for (auto& [cid, s] : impl_->sessions_) {
            if (s.role == "engineer" && s.mode == SessionMode::kOnService)
                s.mode = SessionMode::kObserver;
        }
        impl_->BroadcastEvent(ServiceEndedEvent{}, connection_id);
        spdlog::info("Admin '{}' disconnected — SERVICE_ENDED broadcast",
                     state.user_id);
    }
    impl_->sessions_.erase(it);
    impl_->subscribers_.erase(connection_id);
}

void buf_connect_server::session::SessionManager::Subscribe(
        const std::string& connection_id,
        connect::ConnectResponseWriter* writer) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->subscribers_[connection_id] = writer;
}

void buf_connect_server::session::SessionManager::Unsubscribe(
        const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->subscribers_.erase(connection_id);
}

buf_connect_server::session::ConflictResolutionResult
buf_connect_server::session::SessionManager::ClaimActiveRole(
        const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto it = impl_->sessions_.find(connection_id);
    if (it == impl_->sessions_.end()) return {false, "session not found"};

    auto& state = it->second;
    if (state.role != "engineer") return {false, "not an engineer"};

    for (auto& [cid, s] : impl_->sessions_) {
        if (cid != connection_id && s.role == "engineer" &&
            s.mode == SessionMode::kActive)
            return {false, "role already taken"};
    }
    state.mode = SessionMode::kActive;
    impl_->BroadcastEvent(RoleClaimedEvent{state.user_id}, connection_id);
    spdlog::info("Engineer '{}' claimed ACTIVE role", state.user_id);
    return {true, ""};
}

void buf_connect_server::session::SessionManager::AdminConflictResponse(
        const std::string& connection_id, const std::string& choice) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (choice == "GRANT") {
        impl_->active_admin_connection_id_.reset();
        auto it = impl_->sessions_.find(connection_id);
        if (it != impl_->sessions_.end()) {
            impl_->SendEventTo(connection_id,
                               ForcedLogoutEvent{"Admin granted session to incoming admin"});
            impl_->sessions_.erase(it);
            impl_->subscribers_.erase(connection_id);
        }
    } else if (choice == "SNOOZE") {
        spdlog::info("Admin on connection '{}' snoozed conflict", connection_id);
    }
    impl_->BroadcastEvent(ConflictResolvedEvent{choice});
}

buf_connect_server::session::SessionState
buf_connect_server::session::SessionManager::GetSession(
        const std::string& connection_id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto it = impl_->sessions_.find(connection_id);
    if (it == impl_->sessions_.end()) return {};
    return it->second;
}

void buf_connect_server::session::SessionManager::UpdateHeartbeat(const std::string& session_id) {
    std::lock_guard lock(impl_->mutex_);
    auto it = impl_->sessions_.find(session_id);
    if (it != impl_->sessions_.end())
        it->second.last_heartbeat = std::chrono::steady_clock::now();
}

void buf_connect_server::session::SessionManager::StartExpiryLoop(int64_t ttl, int grace) {
    impl_->access_token_ttl_ = ttl;
    impl_->expiry_running_   = true;
    impl_->expiry_thread_    = std::thread([this]{ impl_->RunExpiryLoop(); });
}

void buf_connect_server::session::SessionManager::StopExpiryLoop() {
    impl_->expiry_running_ = false;
    if (impl_->expiry_thread_.joinable())
        impl_->expiry_thread_.join();
}
