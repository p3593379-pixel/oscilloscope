// FILE: buf_connect_server/include/buf_connect_server/server.hpp
#ifndef BUF_CONNECT_SERVER_SERVER_HPP
#define BUF_CONNECT_SERVER_SERVER_HPP

#include "buf_connect_server/config/server_config.hpp"
#include "buf_connect_server/auth/user_store.hpp"
#include "buf_connect_server/session/session_manager.hpp"
#include "buf_connect_server/connect/request.hpp"
#include <memory>

namespace buf_connect_server {

    class ServiceHandlerBase {
    public:
        virtual ~ServiceHandlerBase() = default;
        // Returns the fully-qualified service path prefix, e.g.
        // "/oscilloscope_interface.v2.AuthService"
        virtual std::string ServicePath() const = 0;
        // Register all RPC routes onto the router
        virtual void RegisterRoutes(class BufConnectServer& server) = 0;
    };

    class BufConnectServer {
    public:
        explicit BufConnectServer(ServerConfig config);
        ~BufConnectServer();

        void RegisterService(std::shared_ptr<ServiceHandlerBase> handler);

        auth::UserStore&           GetUserStore();
        session::SessionManager&   GetSessionManager();

        // Register a route on the control plane router
        void RegisterControlRoute(const std::string& path,
                                  std::function<void(const connect::ParsedConnectRequest&,
                                                     connect::ConnectResponseWriter&)> handler);

        // Register a route on the data plane router
        void RegisterDataRoute(const std::string& path,
                               std::function<void(const connect::ParsedConnectRequest&,
                                                  connect::ConnectResponseWriter&)> handler);

        const ServerConfig& GetConfig() const;

        // Blocks until SIGTERM/SIGINT
        void Start();
        void Stop();

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}  // namespace buf_connect_server

#endif  // BUF_CONNECT_SERVER_SERVER_HPP
