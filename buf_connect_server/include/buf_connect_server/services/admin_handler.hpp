#ifndef OSCILLOSCOPE_ADMIN_HANDLER_HPP
#define OSCILLOSCOPE_ADMIN_HANDLER_HPP

#include "buf_connect_server/server.hpp"
#include "buf_connect_server/auth/user_store.hpp"
#include "buf_connect_server/auth/middleware.hpp"
#include "buf_connect_server/config/server_config.hpp"
#include <memory>

namespace buf_connect_server::services {

// Handles ListUsers, CreateUser, DeleteUser, ResetPassword RPCs.
// All operations require admin role; enforced via JWT middleware.
// Registered automatically by BufConnectServer::Start().
    class AdminHandler final : public ServiceHandlerBase {
    public:
        AdminHandler(auth::UserStore&  user_store,
                     const AuthConfig& auth_config);

        std::string ServicePath() const override;
        void RegisterRoutes(BufConnectServer& server) override;

    private:
        auth::UserStore&                    user_store_;
        std::shared_ptr<auth::AuthMiddleware> auth_middleware_;

        bool RequireAdmin(const connect::ParsedConnectRequest&,
                          connect::ConnectResponseWriter&) const;

        void HandleListUsers(const connect::ParsedConnectRequest&,
                             connect::ConnectResponseWriter&);
        void HandleCreateUser(const connect::ParsedConnectRequest&,
                              connect::ConnectResponseWriter&);
        void HandleDeleteUser(const connect::ParsedConnectRequest&,
                              connect::ConnectResponseWriter&);
        void HandleResetPassword(const connect::ParsedConnectRequest&,
                                 connect::ConnectResponseWriter&);
    };

}  // namespace buf_connect_server::services

#endif //OSCILLOSCOPE_ADMIN_HANDLER_HPP
