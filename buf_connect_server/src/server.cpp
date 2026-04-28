#include "buf_connect_server/server.hpp"
#include "buf_connect_server/services/auth_handler.hpp"
#include "buf_connect_server/services/session_handler.hpp"
#include "buf_connect_server/services/admin_handler.hpp"
#include "buf_connect_server/config/config_server.hpp"
#include "buf_connect_server/connect/request.hpp"
#include "buf_connect_server/connect/response_writer.hpp"
#include "buf_connect_server/auth/jwt.hpp"
#include "buf_connect_server/auth/middleware.hpp"
#include "buf_connect_server/transport/control_plane.hpp"
#include "buf_connect_server/transport/data_plane.hpp"
#include <spdlog/spdlog.h>
#include <csignal>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

static std::atomic<bool>    g_shutdown{false};
static std::mutex           g_shutdown_mutex;
static std::condition_variable g_shutdown_cv;

static void SignalHandler(int) {
    g_shutdown = true;
    g_shutdown_cv.notify_all();
}

class buf_connect_server::BufConnectServer::Impl {
public:
    ServerConfig                             config_;

    // Owned subsystems
    auth::UserStore                          user_store_;
    session::SessionManager                  session_manager_;
    std::shared_ptr<auth::JwtIssuer>          jwt_issuer_;
    std::shared_ptr<auth::AuthMiddleware>    auth_middleware_;

    // Transport planes (constructed after config is fully set)
    std::unique_ptr<transport::ControlPlane> control_plane_;
    std::unique_ptr<transport::DataPlane>    data_plane_;

    // Config REST server (served via cpp-httplib, not h2c)
    std::unique_ptr<ConfigServer>            config_server_;

    std::vector<std::shared_ptr<ServiceHandlerBase>> services_;

    explicit Impl(ServerConfig cfg)
            : config_(std::move(cfg)),
              user_store_(config_.user_db_path) {
        jwt_issuer_       = std::make_shared<auth::JwtIssuer>(config_.auth.jwt_secret);
        auth_middleware_ = std::make_shared<auth::AuthMiddleware>(jwt_issuer_);
        session_manager_.StartExpiryLoop(config_.session.grace_period_admin_seconds,
                                         config_.session.grace_period_engineer_seconds);

        control_plane_ = std::make_unique<transport::ControlPlane>(config_.control_plane);

        // Data plane only if not single-interface mode
        if (!config_.single_interface_mode) {
            data_plane_ = std::make_unique<transport::DataPlane>(config_.data_plane);
        }

        config_server_ = std::make_unique<ConfigServer>(&config_, &user_store_, "./static/control_panel");
    }
};

buf_connect_server::BufConnectServer::BufConnectServer(ServerConfig config)
        : impl_(std::make_unique<Impl>(std::move(config))) {}

buf_connect_server::BufConnectServer::~BufConnectServer() = default;

void buf_connect_server::BufConnectServer::RegisterService(
        std::shared_ptr<ServiceHandlerBase> handler) {
    handler->RegisterRoutes(*this);
    impl_->services_.push_back(std::move(handler));
}

buf_connect_server::auth::UserStore&
buf_connect_server::BufConnectServer::GetUserStore() {
    return impl_->user_store_;
}

buf_connect_server::session::SessionManager&
buf_connect_server::BufConnectServer::GetSessionManager() {
    return impl_->session_manager_;
}

void buf_connect_server::BufConnectServer::RegisterControlRoute(
        const std::string& path,
        std::function<void(const connect::ParsedConnectRequest&,
                           connect::ConnectResponseWriter&)> handler) {
    impl_->control_plane_->GetRouter().Register(path, std::move(handler));
}

void buf_connect_server::BufConnectServer::RegisterDataRoute(
        const std::string& path,
        std::function<void(const connect::ParsedConnectRequest&,
                           connect::ConnectResponseWriter&)> handler) {
    if (impl_->data_plane_) {
        impl_->data_plane_->GetRouter().Register(path, std::move(handler));
    } else {
        // Single-interface mode: data routes also go on the control plane
        spdlog::warn("Single-interface mode: registering data route '{}' on control plane", path);
        impl_->control_plane_->GetRouter().Register(path, std::move(handler));
    }
}

const buf_connect_server::ServerConfig&
buf_connect_server::BufConnectServer::GetConfig() const {
    return impl_->config_;
}

void buf_connect_server::BufConnectServer::Start()
{
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGINT,  SignalHandler);

    auto& cfg = impl_->config_;

    // Start the config/control-panel REST server on control_plane.port + 1
    impl_->config_server_->Start(cfg.control_plane.bind_address,
                                 static_cast<uint16_t>(cfg.control_plane.port + 1));

    // Auto-register built-in handlers — auth / session / admin
    auto auth_h    = std::make_shared<services::AuthHandler>(
            impl_->user_store_, impl_->config_.auth, impl_->session_manager_);
//    auto session_h = std::make_shared<services::SessionHandler>(
//            impl_->session_manager_, impl_->config_.auth);
//    auto admin_h   = std::make_shared<services::AdminHandler>(
//            impl_->user_store_, impl_->config_.auth);

    auth_h->RegisterRoutes(*this);
//    session_h->RegisterRoutes(*this);
//    admin_h->RegisterRoutes(*this);

    // Start the h2c/h2 transport planes
    impl_->control_plane_->Start();
    if (impl_->data_plane_) {
        impl_->data_plane_->Start();
    }

    if (cfg.single_interface_mode) {
        spdlog::warn("Single-interface mode: data streaming shares control plane bandwidth");
    }

    spdlog::info("BufConnectServer started — control plane {}:{} | data plane {}:{} | config REST {}:{}",
                 cfg.control_plane.bind_address, cfg.control_plane.port,
                 cfg.data_plane.bind_address,    cfg.data_plane.port,
                 cfg.control_plane.bind_address, cfg.control_plane.port + 1);

    std::unique_lock<std::mutex> lock(g_shutdown_mutex);
    g_shutdown_cv.wait(lock, [] { return g_shutdown.load(); });
    spdlog::info("BufConnectServer shutting down");
}

void buf_connect_server::BufConnectServer::Stop() {
    g_shutdown = true;
    g_shutdown_cv.notify_all();
    if (impl_->control_plane_) impl_->control_plane_->Stop();
    if (impl_->data_plane_)    impl_->data_plane_->Stop();
    impl_->config_server_->Stop();
}
