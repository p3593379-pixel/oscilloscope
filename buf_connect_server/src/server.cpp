// FILE: buf_connect_server/src/server.cpp
#include "buf_connect_server/server.hpp"
#include "buf_connect_server/services/auth_handler.hpp"
#include "buf_connect_server/services/session_handler.hpp"
#include "buf_connect_server/services/admin_handler.hpp"
#include "buf_connect_server/config/config_loader.hpp"
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
#include <random>
#include <iomanip>

static std::atomic<bool>       g_shutdown{false};
static std::mutex              g_shutdown_mutex;
static std::condition_variable g_shutdown_cv;

static void SignalHandler(int) {
    g_shutdown = true;
    g_shutdown_cv.notify_all();
}

class buf_connect_server::BufConnectServer::Impl {
public:
    ServerConfig                             config_;
    std::string                              config_path_; // persisted path

    auth::UserStore                          user_store_;
    session::SessionManager                  session_manager_;
    std::shared_ptr<auth::JwtIssuer>         jwt_issuer_;
    std::shared_ptr<auth::AuthMiddleware>    auth_middleware_;

    std::unique_ptr<transport::ControlPlane> control_plane_;
    std::unique_ptr<transport::DataPlane>    data_plane_;
    std::unique_ptr<ConfigServer>            config_server_;

    std::vector<std::shared_ptr<ServiceHandlerBase>> services_;

    explicit Impl(ServerConfig cfg, std::string config_path)
            : config_(std::move(cfg))
            , config_path_(std::move(config_path))
            , user_store_(config_.user_db_path)
    {
        std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<uint64_t> dist;
        std::ostringstream oss;
        auto v1 = dist(rng), v2 = dist(rng);
        oss << std::hex << std::setfill('0')
            << std::setw(8)  << (v1 >> 32)           << "-"
            << std::setw(4)  << ((v1 >> 16) & 0xFFFF) << "-"
            << std::setw(4)  << (v1 & 0xFFFF)          << "-"
            << std::setw(4)  << (v2 >> 48)              << "-"
            << std::setw(12) << (v2 & 0xFFFFFFFFFFFFull);
        auto jwt_secret  = oss.str();
        jwt_issuer_      = std::make_shared<auth::JwtIssuer>(jwt_secret);
        auth_middleware_ = std::make_shared<auth::AuthMiddleware>(jwt_issuer_);

        session_manager_.StartExpiryLoop(config_.session.grace_period_admin_seconds,
                                         config_.session.grace_period_engineer_seconds);

        // Planes constructed with the loaded config values
        control_plane_ = std::make_unique<transport::ControlPlane>(config_.control_plane);
        data_plane_    = std::make_unique<transport::DataPlane>(config_.data_plane);

        // ConfigServer wired with a save-on-update callback and the config path
        config_server_ = std::make_unique<ConfigServer>(
                &config_,
                &user_store_,
                "./static/control_panel_web",
                [this](const ServerConfig& updated) {
                    // The in-memory config has already been updated by ConfigServer.
                    // SaveToFile was already called inside persist(). Nothing extra needed
                    // here unless the caller wants to react (e.g. reconfigure spdlog level).
                    spdlog::debug("Config updated and saved to '{}'", config_path_);
                    (void)updated;
                },
                config_path_
        );
        NginxWriter::Options nginx_opts;
        nginx_opts.conf_path        = "/etc/nginx/conf.d/oscilloscope.conf";
        nginx_opts.nginx_reload_cmd = "sudo /usr/sbin/nginx -s reload";
        config_server_->SetNginxWriter(std::move(nginx_opts));
    }
};

// ─── Factory: load from file, then construct ────────────────────────────────

buf_connect_server::BufConnectServer
buf_connect_server::BufConnectServer::FromFile(const std::string& config_path) {
    ServerConfig cfg = ConfigLoader::LoadFromFile(config_path);
    return BufConnectServer(std::move(cfg), config_path);
}

// ─── Constructors ───────────────────────────────────────────────────────────

buf_connect_server::BufConnectServer::BufConnectServer(ServerConfig config,
                                                       std::string  config_path)
        : impl_(std::make_unique<Impl>(std::move(config), std::move(config_path))) {}

buf_connect_server::BufConnectServer::~BufConnectServer() = default;

// ─── Service registration ───────────────────────────────────────────────────

void buf_connect_server::BufConnectServer::RegisterService(
        std::shared_ptr<ServiceHandlerBase> handler) {
    handler->RegisterRoutes(*this);
    impl_->services_.push_back(std::move(handler));
}

buf_connect_server::auth::UserStore&
buf_connect_server::BufConnectServer::GetUserStore() { return impl_->user_store_; }

buf_connect_server::session::SessionManager&
buf_connect_server::BufConnectServer::GetSessionManager() { return impl_->session_manager_; }

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
        SPDLOG_WARN("Single-interface mode: registering data route '{}' on control plane", path);
        impl_->control_plane_->GetRouter().Register(path, std::move(handler));
    }
}

const buf_connect_server::ServerConfig&
buf_connect_server::BufConnectServer::GetConfig() const { return impl_->config_; }

// ─── Start / Stop ───────────────────────────────────────────────────────────

void buf_connect_server::BufConnectServer::Start(const std::string& _jwt_secret)
{
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGINT,  SignalHandler);

    auto& cfg = impl_->config_;

    // Config/control-panel REST server on control_plane.port + 1
    impl_->config_server_->Start(cfg.control_plane.bind_address,
                                 static_cast<uint16_t>(cfg.control_plane.port + 1));

    // Built-in auth / session handlers
    auto auth_h    = std::make_shared<services::AuthHandler>(
            impl_->user_store_, _jwt_secret, cfg.session, impl_->session_manager_);
    auto session_h = std::make_shared<services::SessionHandler>(
            impl_->session_manager_, _jwt_secret);
    auth_h->RegisterRoutes(*this);
    session_h->RegisterRoutes(*this);

    // Start h2c transport planes using bind_address and port from loaded config
    impl_->control_plane_->Start();
    if (impl_->data_plane_ && cfg.data_plane.enabled) {
        impl_->data_plane_->Start();
    } else {
        spdlog::warn("Data plane disabled — data streaming shares control plane bandwidth");
    }

    spdlog::info("BufConnectServer started — "
                 "control {}:{} | data {}:{} | config-REST {}:{} | config-file '{}'",
                 cfg.control_plane.bind_address, cfg.control_plane.port,
                 cfg.data_plane.bind_address,    cfg.data_plane.port,
                 cfg.control_plane.bind_address, cfg.control_plane.port + 1,
                 impl_->config_path_.empty() ? "(in-memory)" : impl_->config_path_);

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
