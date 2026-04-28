#ifndef OSCILLOSCOPE_SESSION_HANDLER_HPP
#define OSCILLOSCOPE_SESSION_HANDLER_HPP

#include "buf_connect_server/server.hpp"
#include "buf_connect_server/session/session_manager.hpp"
#include "buf_connect_server/auth/stream_token.hpp"
#include "buf_connect_server/auth/middleware.hpp"
#include "buf_connect_server/config/server_config.hpp"
#include <memory>

namespace buf_connect_server::services {

// Handles WatchSessionEvents, GetStreamToken, ClaimActiveRole,
// and AdminConflictResponse RPCs.
// Registered automatically by BufConnectServer::Start().
    class SessionHandler final : public ServiceHandlerBase {
    public:
        SessionHandler(session::SessionManager& mgr, const AuthConfig& auth_config);

        std::string ServicePath() const override;
        void RegisterRoutes(BufConnectServer& server) override;

    private:
        session::SessionManager&                mgr_;
        AuthConfig                              auth_config_;
        std::shared_ptr<auth::StreamToken>      stream_token_;
        std::shared_ptr<auth::AuthMiddleware>   auth_middleware_;

        void HandleWatchSessionEvents(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& w);
        void HandleGetStreamToken(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& w);
        void HandleClaimActiveRole(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& w);
        void HandleAdminConflict(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& w);
        void HandleHeartbeat(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& w);
    };

}  // namespace buf_connect_server::services


#endif //OSCILLOSCOPE_SESSION_HANDLER_HPP
