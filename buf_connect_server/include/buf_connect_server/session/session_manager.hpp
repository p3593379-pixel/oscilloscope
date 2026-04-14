#ifndef BUF_CONNECT_SERVER_SESSION_SESSION_MANAGER_HPP
#define BUF_CONNECT_SERVER_SESSION_SESSION_MANAGER_HPP

#include "buf_connect_server/session/session_event.hpp"
#include "buf_connect_server/connect/response_writer.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace buf_connect_server::session {

    enum class SessionMode {
        kUnspecified,
        kActiveAdmin,
        kActive,
        kObserver,
        kOnService,
    };

    struct SessionState {
        std::string connection_id;
        std::string user_id;
        std::string role;
        SessionMode mode = SessionMode::kObserver;
        std::chrono::steady_clock::time_point last_heartbeat = std::chrono::steady_clock::now();
    };

    struct ConflictResolutionResult {
        bool    accepted = false;
        std::string reason;
    };

    class SessionManager {
    public:
        SessionManager();
        ~SessionManager();

        // Called when a client connects. Returns the assigned session mode.
        SessionMode Connect(const std::string& connection_id,
                            const std::string& user_id,
                            const std::string& role);

        // Called when a client disconnects.
        void Disconnect(const std::string& connection_id);

        // Subscribe a ConnectResponseWriter to receive SessionEvent stream frames.
        void Subscribe(const std::string& connection_id,
                       connect::ConnectResponseWriter* writer);

        void Unsubscribe(const std::string& connection_id);

        // Active engineer claims the ACTIVE role when vacant.
        ConflictResolutionResult ClaimActiveRole(const std::string& connection_id);

        // Existing admin responds to a conflict.
        void AdminConflictResponse(const std::string& connection_id,
                                   const std::string& choice);

        SessionState GetSession(const std::string& connection_id) const;

        void UpdateHeartbeat(const std::string& session_id);
        void StartExpiryLoop(int64_t access_token_ttl_seconds, int grace_seconds = 30);
        void StopExpiryLoop();


    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}  // namespace buf_connect_server::session

#endif  // BUF_CONNECT_SERVER_SESSION_SESSION_MANAGER_HPP
