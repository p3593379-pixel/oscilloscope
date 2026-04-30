#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "buf_connect_server/auth/jwt.hpp"
#include "buf_connect_server/auth/user_store.hpp"
#include "buf_connect_server/config/server_config.hpp"
#include "buf_connect_server/session/session_manager.hpp"
#include "buf_connect_server/server.hpp"

namespace buf_connect_server {
    namespace services {

        class AuthHandler {
        public:
            AuthHandler(auth::UserStore&         user_store,
                        const std::string & _jwt_secret,
                        const SessionConfig & _session_config,
                        session::SessionManager& mgr);

            // Processes a Login RPC / HTTP request.
            // Returns HTTP status code; writes response body via the response writer.
            void HandleLogin(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& writer);

            // Processes a RenewCallToken RPC.
            // Reads the session_ticket cookie, validates it, issues a new call_token,
            // rotates the cookie, and calls mgr_.Touch.
            void HandleRenewCallToken(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& writer);

            // Processes a TakeOver RPC.
            // Reads the session_ticket cookie (issued on conflict path of HandleLogin),
            // validates it, evicts the incumbent session, and issues fresh tokens.
            void HandleTakeOver(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& writer);

            void RegisterRoutes(BufConnectServer& server);
        private:
            // Tracks a pending (conflicting) login so TakeOver can find it.
            struct PendingLogin {
                session::UserUuid user_id;
                session::SessionUuid new_session_id;
                std::string role;
            };

            // Issues a call_token JWT (type = "call_token").
            std::string IssueCallToken(const session::UserUuid &_user_uuid,
                                       const std::string& _role,
                                       const session::SessionUuid &_session_uuid) const;


            // Extracts a call token value by name from an authorization bearer
            static std::string ExtractAuthorizationBearer(const connect::ParsedConnectRequest& req);

            auth::UserStore&         user_store_;
            const SessionConfig &    kSessionConfig;
            session::SessionManager& mgr_;
            std::shared_ptr<auth::JwtIssuer> jwt_issuer_;

            std::mutex                                    pending_mutex_;
            std::unordered_map<std::string, PendingLogin> pending_logins_;  // keyed by new_session_id
        };

    }  // namespace services
}  // namespace buf_connect_server
