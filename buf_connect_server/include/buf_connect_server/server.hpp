// FILE: buf_connect_server/include/buf_connect_server/server.hpp
#ifndef BUF_CONNECT_SERVER_SERVER_HPP
#define BUF_CONNECT_SERVER_SERVER_HPP

#include "buf_connect_server/config/server_config.hpp"
#include "buf_connect_server/auth/user_store.hpp"
#include "buf_connect_server/session/session_manager.hpp"
#include "buf_connect_server/connect/request.hpp"
#include "buf_connect_server/connect/response_writer.hpp"
#include <memory>
#include <string>

namespace buf_connect_server {

    class ServiceHandlerBase {
    public:
        ServiceHandlerBase()  = default;
        virtual ~ServiceHandlerBase() = default;
        [[nodiscard]] virtual std::string ServicePath() const = 0;
        virtual void RegisterRoutes(class BufConnectServer& server) = 0;
    };

    class BufConnectServer {
    public:
        // Preferred entry point: load config from file (creates file with
        // defaults if it doesn't exist), then construct the server.
        static BufConnectServer FromFile(const std::string& config_path);

        // Direct construction — config already in hand.
        // config_path is optional; pass empty string for in-memory-only operation.
        explicit BufConnectServer(ServerConfig config,
                                  std::string  config_path = "");
        ~BufConnectServer();

        void RegisterService(std::shared_ptr<ServiceHandlerBase> handler);

        auth::UserStore&         GetUserStore();
        session::SessionManager& GetSessionManager();

        void RegisterControlRoute(
                const std::string& path,
                std::function<void(const connect::ParsedConnectRequest&,
                                   connect::ConnectResponseWriter&)> handler);

        void RegisterDataRoute(
                const std::string& path,
                std::function<void(const connect::ParsedConnectRequest&,
                                   connect::ConnectResponseWriter&)> handler);

        const ServerConfig& GetConfig() const;

        // Blocks until SIGTERM/SIGINT
        void Start(const std::string& jwt_secret);
        void Stop();

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}  // namespace buf_connect_server

#endif  // BUF_CONNECT_SERVER_SERVER_HPP
