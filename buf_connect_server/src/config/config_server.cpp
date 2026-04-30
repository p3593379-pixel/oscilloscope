// FILE: buf_connect_server/src/config/config_server.cpp
#include "buf_connect_server/config/config_server.hpp"
#include "buf_connect_server/config/config_loader.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <filesystem>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>

using json = nlohmann::json;

struct DownloadEntry {
    std::string file_path;
    int64_t     expires_at;
};

class buf_connect_server::ConfigServer::Impl {
public:
    httplib::Server        svr_;
    ServerConfig*          config_;
    auth::UserStore*       user_store_;
    std::string            static_dir_;
    ConfigUpdateCallback   on_update_;
    std::mutex             token_mutex_;
    std::unordered_map<std::string, DownloadEntry> download_tokens_;
    std::thread            thread_;

    // Live metrics counters (updated externally via callbacks / future API)
    std::atomic<uint64_t>  bytes_streamed_{0};
    std::atomic<uint32_t>  active_sessions_{0};
    std::atomic<uint64_t>  total_sessions_{0};
    std::atomic<uint32_t>  peak_sessions_{0};
    std::chrono::steady_clock::time_point start_time_{std::chrono::steady_clock::now()};
};

buf_connect_server::ConfigServer::ConfigServer(
        ServerConfig* config,
        auth::UserStore* user_store,
        const std::string& static_dir,
        ConfigUpdateCallback on_update) : impl_(std::make_unique<Impl>())
{
    impl_->config_     = config;
    impl_->user_store_ = user_store;
    impl_->static_dir_ = static_dir;
    impl_->on_update_  = std::move(on_update);

    auto& svr = impl_->svr_;

    // Serve static files
    if (!static_dir.empty() && std::filesystem::exists(static_dir))
        svr.set_mount_point("/", static_dir);

    // -------------------------------------------------------------------------
    // GET /api/config  — full config
    // -------------------------------------------------------------------------
    svr.Get("/api/config", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(ConfigLoader::ToJson(*impl_->config_), "application/json");
    });

    // PUT /api/config  — replace full config
    svr.Put("/api/config", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto updated = ConfigLoader::FromJson(req.body);
            *impl_->config_ = updated;
            if (impl_->on_update_) impl_->on_update_(updated);
            res.set_content(ConfigLoader::ToJson(updated), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // -------------------------------------------------------------------------
    // GET /api/config/network
    // -------------------------------------------------------------------------
    svr.Get("/api/config/network", [this](const httplib::Request&, httplib::Response& res) {
        auto full = json::parse(ConfigLoader::ToJson(*impl_->config_));
        json j = {
                {"control_plane", full["control_plane"]},
                {"data_plane",    full["data_plane"]}
        };
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/network — requires restart
    svr.Put("/api/config/network", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            auto& c = *impl_->config_;

            auto patch_iface = [](const json& src, InterfaceConfig& dst) {
                dst.bind_address = src.value("bind_address", dst.bind_address);
                dst.port         = src.value("port",         dst.port);
                dst.enabled      = src.value("enabled",      dst.enabled);
                if (src.contains("tls")) {
                    dst.tls.enabled     = src["tls"].value("enabled",     dst.tls.enabled);
                    dst.tls.cert_path   = src["tls"].value("cert_path",   dst.tls.cert_path);
                    dst.tls.key_path    = src["tls"].value("key_path",    dst.tls.key_path);
                    dst.tls.min_version = src["tls"].value("min_version", dst.tls.min_version);
                }
            };

            if (j.contains("control_plane")) patch_iface(j["control_plane"], c.control_plane);
            if (j.contains("data_plane"))    patch_iface(j["data_plane"],    c.data_plane);

            if (impl_->on_update_) impl_->on_update_(c);

            auto full = json::parse(ConfigLoader::ToJson(c));
            json resp = {
                    {"control_plane",    full["control_plane"]},
                    {"data_plane",       full["data_plane"]},
                    {"restart_required", true}
            };
            res.status = 202;
            res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad request\"}", "application/json");
        }
    });

    // -------------------------------------------------------------------------
    // GET /api/config/session
    // -------------------------------------------------------------------------
    svr.Get("/api/config/session", [this](const httplib::Request&, httplib::Response& res) {
        const auto& s = impl_->config_->session;
        json j = {
                {"admin_conflict_timeout_seconds", s.admin_conflict_timeout_seconds},
                {"snooze_duration_seconds",        s.snooze_duration_seconds},
                {"max_concurrent_sessions",        s.max_concurrent_sessions},
                {"call_token_renew_period",        s.call_token_renew_period},
                {"grace_period_admin_seconds",     s.grace_period_admin_seconds},
                {"grace_period_engineer_seconds",  s.grace_period_engineer_seconds}
        };
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/session
    svr.Put("/api/config/session", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j  = json::parse(req.body);
            auto& s = impl_->config_->session;
            s.admin_conflict_timeout_seconds =
                    j.value("admin_conflict_timeout_seconds", s.admin_conflict_timeout_seconds);
            s.snooze_duration_seconds =
                    j.value("snooze_duration_seconds",        s.snooze_duration_seconds);
            s.max_concurrent_sessions =
                    j.value("max_concurrent_sessions",        s.max_concurrent_sessions);
            s.call_token_renew_period =
                    j.value("call_token_renew_period",        s.call_token_renew_period);
            s.grace_period_admin_seconds =
                    j.value("grace_period_admin_seconds",     s.grace_period_admin_seconds);
            s.grace_period_engineer_seconds =
                    j.value("grace_period_engineer_seconds",  s.grace_period_engineer_seconds);

            if (impl_->on_update_) impl_->on_update_(*impl_->config_);
            json resp = {
                    {"admin_conflict_timeout_seconds", s.admin_conflict_timeout_seconds},
                    {"snooze_duration_seconds",        s.snooze_duration_seconds},
                    {"max_concurrent_sessions",        s.max_concurrent_sessions},
                    {"call_token_renew_period",        s.call_token_renew_period},
                    {"grace_period_admin_seconds",     s.grace_period_admin_seconds},
                    {"grace_period_engineer_seconds",  s.grace_period_engineer_seconds}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad request\"}", "application/json");
        }
    });

    // -------------------------------------------------------------------------
    // GET /api/config/streaming
    // -------------------------------------------------------------------------
    svr.Get("/api/config/streaming", [this](const httplib::Request&, httplib::Response& res) {
        const auto& s = impl_->config_->streaming;
        json j = {
                {"compression_enabled",   s.compression_enabled},
                {"compression_algorithm", s.compression_algorithm}
        };
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/streaming
    svr.Put("/api/config/streaming", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j  = json::parse(req.body);
            auto& s = impl_->config_->streaming;
            s.compression_enabled   = j.value("compression_enabled",   s.compression_enabled);
            s.compression_algorithm = j.value("compression_algorithm", s.compression_algorithm);
            if (impl_->on_update_) impl_->on_update_(*impl_->config_);
            json resp = {
                    {"compression_enabled",   s.compression_enabled},
                    {"compression_algorithm", s.compression_algorithm}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad request\"}", "application/json");
        }
    });

    // -------------------------------------------------------------------------
    // GET /api/config/log
    // -------------------------------------------------------------------------
    svr.Get("/api/config/log", [this](const httplib::Request&, httplib::Response& res) {
        const auto& l = impl_->config_->log;
        json j = {
                {"level",                l.level},
                {"log_file_name_prefix", l.log_file_name_prefix},
                {"console",              l.console},
                {"max_size_mb",          l.max_size_mb},
                {"max_files",            l.max_files}
        };
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/log — applied immediately, no restart
    svr.Put("/api/config/log", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j  = json::parse(req.body);
            auto& l = impl_->config_->log;
            l.level                = j.value("level",                l.level);
            l.log_file_name_prefix = j.value("log_file_name_prefix", l.log_file_name_prefix);
            l.console              = j.value("console",              l.console);
            l.max_size_mb          = j.value("max_size_mb",          l.max_size_mb);
            l.max_files            = j.value("max_files",            l.max_files);
            // Apply level change immediately
            spdlog::set_level(spdlog::level::from_str(l.level));
            if (impl_->on_update_) impl_->on_update_(*impl_->config_);
            json resp = {
                    {"level",                l.level},
                    {"log_file_name_prefix", l.log_file_name_prefix},
                    {"console",              l.console},
                    {"max_size_mb",          l.max_size_mb},
                    {"max_files",            l.max_files}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad request\"}", "application/json");
        }
    });

    // -------------------------------------------------------------------------
    // GET /api/config/metrics
    // -------------------------------------------------------------------------
    svr.Get("/api/config/metrics", [this](const httplib::Request&, httplib::Response& res) {
        const auto& m = impl_->config_->metrics;
        json j = {
                {"enabled",                 m.enabled},
                {"metrics_log_path",        m.metrics_log_path},
                {"metrics_log_file_prefix", m.metrics_log_file_prefix}
        };
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/metrics
    svr.Put("/api/config/metrics", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j  = json::parse(req.body);
            auto& m = impl_->config_->metrics;
            m.enabled                 = j.value("enabled",                 m.enabled);
            m.metrics_log_path        = j.value("metrics_log_path",        m.metrics_log_path);
            m.metrics_log_file_prefix = j.value("metrics_log_file_prefix", m.metrics_log_file_prefix);
            if (impl_->on_update_) impl_->on_update_(*impl_->config_);
            json resp = {
                    {"enabled",                 m.enabled},
                    {"metrics_log_path",        m.metrics_log_path},
                    {"metrics_log_file_prefix", m.metrics_log_file_prefix}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad request\"}", "application/json");
        }
    });

    // -------------------------------------------------------------------------
    // GET /api/metrics/live — live runtime metrics
    // -------------------------------------------------------------------------
    svr.Get("/api/metrics/live", [this](const httplib::Request&, httplib::Response& res) {
        auto now     = std::chrono::steady_clock::now();
        auto uptime  = std::chrono::duration_cast<std::chrono::seconds>(
                now - impl_->start_time_).count();
        json j = {
                {"uptime_seconds",   uptime},
                {"active_sessions",  impl_->active_sessions_.load()},
                {"total_sessions",   impl_->total_sessions_.load()},
                {"peak_sessions",    impl_->peak_sessions_.load()},
                {"bytes_streamed",   impl_->bytes_streamed_.load()},
                {"active_streams",   impl_->active_sessions_.load()},
                // RPS stub — replace with a real sliding-window counter
                {"rps",              0.0}
        };
        res.set_content(j.dump(), "application/json");
    });

    // -------------------------------------------------------------------------
    // Users endpoints  (unchanged — kept verbatim)
    // -------------------------------------------------------------------------
    svr.Get("/api/users", [this](const httplib::Request&, httplib::Response& res) {
        if (!impl_->user_store_) {
            res.status = 503;
            res.set_content("{\"error\":\"user store unavailable\"}", "application/json");
            return;
        }
        auto users = impl_->user_store_->ListUsers();
        json arr   = json::array();
        for (const auto& u : users) {
            arr.push_back({
                                  {"user_id",    u.uuid},
                                  {"username",   u.username},
                                  {"role",       u.role},
                                  {"created_at", u.created_at},
                                  {"last_login", u.last_login}
                          });
        }
        res.set_content(json{{"users", arr}}.dump(), "application/json");
    });

    svr.Post("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
        if (!impl_->user_store_) { res.status = 503; res.set_content("{\"error\":\"user store unavailable\"}", "application/json"); return; }
        try {
            auto j            = json::parse(req.body);
            std::string uname = j.value("username", "");
            std::string pwd   = j.value("password", "");
            std::string role  = j.value("role",     "engineer");
            if (uname.empty() || pwd.empty()) { res.status = 400; res.set_content("{\"error\":\"username and password required\"}", "application/json"); return; }
            if (role != "admin" && role != "engineer") { res.status = 400; res.set_content("{\"error\":\"role must be admin or engineer\"}", "application/json"); return; }
            auto user = impl_->user_store_->CreateUser(uname, pwd, role);
            if (!user) { res.status = 409; res.set_content("{\"error\":\"username already exists\"}", "application/json"); return; }
            json resp = {{"user_id", user->uuid}, {"username", user->username}, {"role", user->role}, {"created_at", user->created_at}, {"last_login", user->last_login}};
            res.set_content(resp.dump(), "application/json");
        } catch (...) { res.status = 400; res.set_content("{\"error\":\"bad request\"}", "application/json"); }
    });

    svr.Delete("/api/users/:id", [this](const httplib::Request& req, httplib::Response& res) {
        if (!impl_->user_store_) { res.status = 503; res.set_content("{\"error\":\"user store unavailable\"}", "application/json"); return; }
        bool ok = impl_->user_store_->DeleteUser(req.path_params.at("id"));
        if (!ok) { res.status = 404; res.set_content("{\"error\":\"user not found\"}", "application/json"); return; }
        res.status = 204;
    });

    svr.Put("/api/users/:id/password", [this](const httplib::Request& req, httplib::Response& res) {
        if (!impl_->user_store_) { res.status = 503; res.set_content("{\"error\":\"user store unavailable\"}", "application/json"); return; }
        try {
            auto j = json::parse(req.body);
            std::string pwd = j.value("new_password", "");
            if (pwd.empty()) { res.status = 400; res.set_content("{\"error\":\"new_password required\"}", "application/json"); return; }
            bool ok = impl_->user_store_->ResetPassword(req.path_params.at("id"), pwd);
            if (!ok) { res.status = 404; res.set_content("{\"error\":\"user not found\"}", "application/json"); return; }
            res.set_content("{\"success\":true}", "application/json");
        } catch (...) { res.status = 400; res.set_content("{\"error\":\"bad request\"}", "application/json"); }
    });

    // -------------------------------------------------------------------------
    // GET /api/status
    // -------------------------------------------------------------------------
    svr.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - impl_->start_time_).count();
        json j = {
                {"uptime_seconds",  uptime},
                {"active_sessions", impl_->active_sessions_.load()},
                {"bytes_streamed",  impl_->bytes_streamed_.load()},
                {"version",         "0.1.0"}
        };
        res.set_content(j.dump(), "application/json");
    });

    // -------------------------------------------------------------------------
    // GET /api/status/interfaces — system network interfaces (for IP picker)
    // -------------------------------------------------------------------------
    svr.Get("/api/status/interfaces", [](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == 0) {
            for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
                char addr_buf[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET,
                          &reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr,
                          addr_buf, sizeof(addr_buf));
                arr.push_back({
                                      {"name",       ifa->ifa_name},
                                      {"status",     (ifa->ifa_flags & IFF_UP) ? "UP" : "DOWN"},
                                      {"address",    addr_buf},
                                      {"speed_mbps", 0}
                              });
            }
            freeifaddrs(ifaddr);
        }
        res.set_content(arr.dump(), "application/json");
    });

    // -------------------------------------------------------------------------
    // Config export / import
    // -------------------------------------------------------------------------
    svr.Post("/api/config/export", [this](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=\"config.json\"");
        res.set_content(ConfigLoader::ToJson(*impl_->config_), "application/json");
    });

    svr.Post("/api/config/import", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto validated = ConfigLoader::FromJson(req.body);
            (void)validated; // dry-run only — caller applies via PUT /api/config
            res.set_content("{\"valid\":true}", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Post("/api/restart", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"restarting\":true}", "application/json");
    });

    // Download token endpoint (unchanged)
    svr.Get("/api/download/:token", [this](const httplib::Request& req, httplib::Response& res) {
        auto token = req.path_params.at("token");
        std::lock_guard<std::mutex> lock(impl_->token_mutex_);
        auto it = impl_->download_tokens_.find(token);
        if (it == impl_->download_tokens_.end()) { res.status = 404; res.set_content("{\"error\":\"token not found\"}", "application/json"); return; }
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        if (now > it->second.expires_at) { impl_->download_tokens_.erase(it); res.status = 410; res.set_content("{\"error\":\"token expired\"}", "application/json"); return; }
        std::ifstream f(it->second.file_path, std::ios::binary);
        if (!f) { res.status = 404; res.set_content("{\"error\":\"file not found\"}", "application/json"); return; }
        std::string body((std::istreambuf_iterator<char>(f)), {});
        res.set_header("Content-Disposition", "attachment; filename=\"download\"");
        res.set_content(body, "application/octet-stream");
    });
}

buf_connect_server::ConfigServer::~ConfigServer() = default;

void buf_connect_server::ConfigServer::Start(const std::string& host, uint16_t port) {
    impl_->thread_ = std::thread([this, host, port]() {
        spdlog::info("ConfigServer listening on {}:{}", host, port);
        impl_->svr_.listen(host.c_str(), port);
    });
}

void buf_connect_server::ConfigServer::Stop() {
    impl_->svr_.stop();
    if (impl_->thread_.joinable()) impl_->thread_.join();
}

void buf_connect_server::ConfigServer::RegisterDownloadToken(
        const std::string& token, const std::string& file_path, uint32_t ttl_seconds) {
    auto expires = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now() + std::chrono::seconds(ttl_seconds));
    std::lock_guard<std::mutex> lock(impl_->token_mutex_);
    impl_->download_tokens_[token] = {file_path, expires};
}