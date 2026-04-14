#ifndef OSCILLOSCOPE_AUTH_HANDLER_HPP
#define OSCILLOSCOPE_AUTH_HANDLER_HPP

#include "buf_connect_server/server.hpp"
#include "buf_connect_server/auth/user_store.hpp"
#include "buf_connect_server/auth/jwt.hpp"
#include "buf_connect_server/config/server_config.hpp"
#include <memory>

namespace buf_connect_server::services {

// Handles Login and Refresh RPCs.
// Registered automatically by BufConnectServer::Start().
    class AuthHandler final : public ServiceHandlerBase {
    public:
        AuthHandler(auth::UserStore&  user_store,
                    const AuthConfig& auth_config);

        std::string ServicePath() const override;
        void RegisterRoutes(BufConnectServer& server) override;

    private:
        auth::UserStore&                    user_store_;
        AuthConfig                          auth_config_;
        std::shared_ptr<auth::JwtUtils>     jwt_utils_;

        void HandleLogin(const connect::ParsedConnectRequest&,
                         connect::ConnectResponseWriter&);
        void HandleRefresh(const connect::ParsedConnectRequest&,
                           connect::ConnectResponseWriter&);
    };

}  // namespace buf_connect_server::services


#endif //OSCILLOSCOPE_AUTH_HANDLER_HPP
