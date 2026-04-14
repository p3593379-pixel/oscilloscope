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
    if (!static_dir.empty() && std::filesystem::exists(static_dir)) {
        svr.set_mount_point("/", static_dir);
    }

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
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"invalid config\"}", "application/json");
        }
    });

    // -------------------------------------------------------------------------
    // GET /api/config/network
    // -------------------------------------------------------------------------
    svr.Get("/api/config/network", [this](const httplib::Request&, httplib::Response& res) {
        auto full = json::parse(ConfigLoader::ToJson(*impl_->config_));
        json j = {
                {"control_plane",         full["control_plane"]},
                {"data_plane",            full["data_plane"]},
                {"single_interface_mode", impl_->config_->single_interface_mode}
        };
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/network — requires restart
    svr.Put("/api/config/network", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            auto& c = *impl_->config_;
            if (j.contains("control_plane")) {
                auto& cp = j["control_plane"];
                c.control_plane.bind_address = cp.value("bind_address", c.control_plane.bind_address);
                c.control_plane.port         = cp.value("port",         c.control_plane.port);
                c.control_plane.enabled      = cp.value("enabled",      c.control_plane.enabled);
                if (cp.contains("tls")) {
                    c.control_plane.tls.enabled     = cp["tls"].value("enabled",     c.control_plane.tls.enabled);
                    c.control_plane.tls.cert_path   = cp["tls"].value("cert_path",   c.control_plane.tls.cert_path);
                    c.control_plane.tls.key_path    = cp["tls"].value("key_path",    c.control_plane.tls.key_path);
                    c.control_plane.tls.min_version = cp["tls"].value("min_version", c.control_plane.tls.min_version);
                }
            }
            if (j.contains("data_plane")) {
                auto& dp = j["data_plane"];
                c.data_plane.bind_address = dp.value("bind_address", c.data_plane.bind_address);
                c.data_plane.port         = dp.value("port",         c.data_plane.port);
                c.data_plane.enabled      = dp.value("enabled",      c.data_plane.enabled);
                if (dp.contains("tls")) {
                    c.data_plane.tls.enabled     = dp["tls"].value("enabled",     c.data_plane.tls.enabled);
                    c.data_plane.tls.cert_path   = dp["tls"].value("cert_path",   c.data_plane.tls.cert_path);
                    c.data_plane.tls.key_path    = dp["tls"].value("key_path",    c.data_plane.tls.key_path);
                    c.data_plane.tls.min_version = dp["tls"].value("min_version", c.data_plane.tls.min_version);
                }
            }
            if (j.contains("single_interface_mode"))
                c.single_interface_mode = j["single_interface_mode"].get<bool>();

            if (impl_->on_update_) impl_->on_update_(c);

            auto full = json::parse(ConfigLoader::ToJson(c));
            json resp = {
                    {"control_plane",         full["control_plane"]},
                    {"data_plane",            full["data_plane"]},
                    {"single_interface_mode", c.single_interface_mode},
                    {"restart_required",      true}
            };
            res.status = 202;
            res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad request\"}", "application/json");
        }
    });

    // -------------------------------------------------------------------------
    // GET /api/config/auth  — jwt_secret intentionally omitted
    // -------------------------------------------------------------------------
    svr.Get("/api/config/auth", [this](const httplib::Request&, httplib::Response& res) {
        const auto& a = impl_->config_->auth;
        json j = {
                {"access_token_ttl_seconds",  a.access_token_ttl_seconds},
                {"refresh_token_ttl_seconds", a.refresh_token_ttl_seconds},
                {"stream_token_ttl_seconds",  a.stream_token_ttl_seconds}
        };
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/auth
    svr.Put("/api/config/auth", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j  = json::parse(req.body);
            auto& a = impl_->config_->auth;
            a.access_token_ttl_seconds  = j.value("access_token_ttl_seconds",  a.access_token_ttl_seconds);
            a.refresh_token_ttl_seconds = j.value("refresh_token_ttl_seconds", a.refresh_token_ttl_seconds);
            a.stream_token_ttl_seconds  = j.value("stream_token_ttl_seconds",  a.stream_token_ttl_seconds);
            // Accept jwt_secret inline (used by AuthSettingsPanel's applySecret path)
            if (j.contains("jwt_secret") && j["jwt_secret"].is_string()) {
                const std::string secret = j["jwt_secret"].get<std::string>();
                if (secret.size() < 32) {
                    res.status = 400;
                    res.set_content("{\"error\":\"jwt_secret must be at least 32 characters\"}", "application/json");
                    return;
                }
                a.jwt_secret = secret;
            }
            if (impl_->on_update_) impl_->on_update_(*impl_->config_);
            json resp = {
                    {"access_token_ttl_seconds",  a.access_token_ttl_seconds},
                    {"refresh_token_ttl_seconds", a.refresh_token_ttl_seconds},
                    {"stream_token_ttl_seconds",  a.stream_token_ttl_seconds}
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
                {"max_clients",                 s.max_clients},
                {"frame_size_bytes",            s.frame_size_bytes},
                {"compression_enabled",         s.compression_enabled},
                {"compression_algorithm",       s.compression_algorithm},
                {"compression_threshold_bytes", s.compression_threshold_bytes},
                {"backpressure_policy",         s.backpressure_policy}
        };
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/streaming
    svr.Put("/api/config/streaming", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j  = json::parse(req.body);
            auto& s = impl_->config_->streaming;
            s.max_clients                = j.value("max_clients",                 s.max_clients);
            s.frame_size_bytes           = j.value("frame_size_bytes",            s.frame_size_bytes);
            s.compression_enabled        = j.value("compression_enabled",         s.compression_enabled);
            s.compression_algorithm      = j.value("compression_algorithm",       s.compression_algorithm);
            s.compression_threshold_bytes= j.value("compression_threshold_bytes", s.compression_threshold_bytes);
            s.backpressure_policy        = j.value("backpressure_policy",         s.backpressure_policy);
            if (impl_->on_update_) impl_->on_update_(*impl_->config_);
            json resp = {
                    {"max_clients",                 s.max_clients},
                    {"frame_size_bytes",            s.frame_size_bytes},
                    {"compression_enabled",         s.compression_enabled},
                    {"compression_algorithm",       s.compression_algorithm},
                    {"compression_threshold_bytes", s.compression_threshold_bytes},
                    {"backpressure_policy",         s.backpressure_policy}
            };
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
                {"max_concurrent_sessions",        s.max_concurrent_sessions}
        };
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/session
    svr.Put("/api/config/session", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j  = json::parse(req.body);
            auto& s = impl_->config_->session;
            s.admin_conflict_timeout_seconds = j.value("admin_conflict_timeout_seconds", s.admin_conflict_timeout_seconds);
            s.snooze_duration_seconds        = j.value("snooze_duration_seconds",        s.snooze_duration_seconds);
            s.max_concurrent_sessions        = j.value("max_concurrent_sessions",        s.max_concurrent_sessions);
            if (impl_->on_update_) impl_->on_update_(*impl_->config_);
            json resp = {
                    {"admin_conflict_timeout_seconds", s.admin_conflict_timeout_seconds},
                    {"snooze_duration_seconds",        s.snooze_duration_seconds},
                    {"max_concurrent_sessions",        s.max_concurrent_sessions}
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
                {"level",       l.level},
                {"destination", l.destination},
                {"file_path",   l.file_path},
                {"max_size_mb", l.max_size_mb},
                {"max_files",   l.max_files},
                {"access_log",  l.access_log}
        };
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/log  — applied immediately, no restart
    svr.Put("/api/config/log", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j  = json::parse(req.body);
            auto& l = impl_->config_->log;
            l.level       = j.value("level",       l.level);
            l.destination = j.value("destination", l.destination);
            l.file_path   = j.value("file_path",   l.file_path);
            l.max_size_mb = j.value("max_size_mb", l.max_size_mb);
            l.max_files   = j.value("max_files",   l.max_files);
            l.access_log  = j.value("access_log",  l.access_log);
            // Apply spdlog level change immediately
            spdlog::set_level(spdlog::level::from_str(l.level));
            if (impl_->on_update_) impl_->on_update_(*impl_->config_);
            json resp = {
                    {"level",       l.level},
                    {"destination", l.destination},
                    {"file_path",   l.file_path},
                    {"max_size_mb", l.max_size_mb},
                    {"max_files",   l.max_files},
                    {"access_log",  l.access_log}
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
        json j = {{"enabled", m.enabled}, {"path", m.path}};
        res.set_content(j.dump(), "application/json");
    });

    // PUT /api/config/metrics
    svr.Put("/api/config/metrics", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j  = json::parse(req.body);
            auto& m = impl_->config_->metrics;
            m.enabled = j.value("enabled", m.enabled);
            m.path    = j.value("path",    m.path);
            if (impl_->on_update_) impl_->on_update_(*impl_->config_);
            json resp = {{"enabled", m.enabled}, {"path", m.path}};
            res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad request\"}", "application/json");
        }
    });

    // GET /api/users
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
                                  {"user_id",    u.id},
                                  {"username",   u.username},
                                  {"role",       u.role},
                                  {"created_at", u.created_at},
                                  {"last_login", u.last_login}
                          });
        }
        res.set_content(json{{"users", arr}}.dump(), "application/json");
    });

    // POST /api/users
    svr.Post("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
        if (!impl_->user_store_) { res.status = 503; res.set_content("{\"error\":\"user store unavailable\"}", "application/json"); return; }
        try {
            auto j            = json::parse(req.body);
            std::string uname = j.value("username", "");
            std::string pwd   = j.value("password", "");
            std::string role  = j.value("role",     "engineer");
            if (uname.empty() || pwd.empty()) {
                res.status = 400;
                res.set_content("{\"error\":\"username and password required\"}", "application/json");
                return;
            }
            if (role != "admin" && role != "engineer") {
                res.status = 400;
                res.set_content("{\"error\":\"role must be admin or engineer\"}", "application/json");
                return;
            }
            auto user = impl_->user_store_->CreateUser(uname, pwd, role);
            if (!user) {
                res.status = 409;
                res.set_content("{\"error\":\"username already exists\"}", "application/json");
                return;
            }
            json resp = {
                    {"user_id",    user->id},
                    {"username",   user->username},
                    {"role",       user->role},
                    {"created_at", user->created_at},
                    {"last_login", user->last_login}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad request\"}", "application/json");
        }
    });

    // DELETE /api/users/:id
    svr.Delete("/api/users/:id", [this](const httplib::Request& req, httplib::Response& res) {
        if (!impl_->user_store_) { res.status = 503; res.set_content("{\"error\":\"user store unavailable\"}", "application/json"); return; }
        auto user_id = req.path_params.at("id");
        bool ok      = impl_->user_store_->DeleteUser(user_id);
        if (!ok) { res.status = 404; res.set_content("{\"error\":\"user not found\"}", "application/json"); return; }
        res.status = 204;
    });

    // PUT /api/users/:id/password
    svr.Put("/api/users/:id/password", [this](const httplib::Request& req, httplib::Response& res) {
        if (!impl_->user_store_) { res.status = 503; res.set_content("{\"error\":\"user store unavailable\"}", "application/json"); return; }
        try {
            auto user_id    = req.path_params.at("id");
            auto j          = json::parse(req.body);
            std::string pwd = j.value("new_password", "");
            if (pwd.empty()) { res.status = 400; res.set_content("{\"error\":\"new_password required\"}", "application/json"); return; }
            bool ok = impl_->user_store_->ResetPassword(user_id, pwd);
            if (!ok) { res.status = 404; res.set_content("{\"error\":\"user not found\"}", "application/json"); return; }
            res.set_content("{\"success\":true}", "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad request\"}", "application/json");
        }
    });

    // -------------------------------------------------------------------------
    // GET /api/status
    // -------------------------------------------------------------------------
    svr.Get("/api/status", [](const httplib::Request&, httplib::Response& res) {
        json j = {
                {"uptime_seconds",  0},
                {"active_sessions", 0},
                {"bytes_streamed",  0},
                {"version",         "0.1.0"}
        };
        res.set_content(j.dump(), "application/json");
    });

    // GET /api/status/interfaces
    svr.Get("/api/status/interfaces", [](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == 0) {
            for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == nullptr) continue;
                if (ifa->ifa_addr->sa_family != AF_INET) continue;
                json iface;
                iface["name"]      = ifa->ifa_name;
                iface["status"]    = (ifa->ifa_flags & IFF_UP) ? "UP" : "DOWN";
                char addr_buf[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET,
                          &reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr,
                          addr_buf, sizeof(addr_buf));
                iface["address"]    = addr_buf;
                iface["speed_mbps"] = 0;
                arr.push_back(iface);
            }
            freeifaddrs(ifaddr);
        }
        res.set_content(arr.dump(), "application/json");
    });

    // -------------------------------------------------------------------------
    // POST /api/config/export
    // -------------------------------------------------------------------------
    svr.Post("/api/config/export", [this](const httplib::Request&, httplib::Response& res) {
        auto body = ConfigLoader::ToJson(*impl_->config_);
        res.set_header("Content-Disposition", "attachment; filename=\"config.json\"");
        res.set_content(body, "application/json");
    });

    // POST /api/config/import  — dry-run validation only
    svr.Post("/api/config/import", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            ConfigLoader::FromJson(req.body);
            res.set_content("{\"valid\":true}", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // -------------------------------------------------------------------------
    // POST /api/restart
    // -------------------------------------------------------------------------
    svr.Post("/api/restart", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"restarting\":true}", "application/json");
        // In production: std::raise(SIGTERM) after flushing response
    });

    // -------------------------------------------------------------------------
    // Archive download endpoint
    // -------------------------------------------------------------------------
    svr.Get("/api/download/:token", [this](const httplib::Request& req, httplib::Response& res) {
        auto token = req.path_params.at("token");
        std::lock_guard<std::mutex> lock(impl_->token_mutex_);
        auto it = impl_->download_tokens_.find(token);
        if (it == impl_->download_tokens_.end()) {
            res.status = 404;
            res.set_content("{\"error\":\"token not found\"}", "application/json");
            return;
        }
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        if (now > it->second.expires_at) {
            impl_->download_tokens_.erase(it);
            res.status = 410;
            res.set_content("{\"error\":\"token expired\"}", "application/json");
            return;
        }
        std::ifstream file(it->second.file_path, std::ios::binary);
        if (!file.is_open()) {
            res.status = 404;
            res.set_content("{\"error\":\"file not found\"}", "application/json");
            return;
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        res.set_header("Content-Disposition",
                       "attachment; filename=\"" +
                       std::filesystem::path(it->second.file_path).filename().string() + "\"");
        res.set_content(content, "application/octet-stream");
        impl_->download_tokens_.erase(it);
    });
}

buf_connect_server::ConfigServer::~ConfigServer() {
    Stop();
}

void buf_connect_server::ConfigServer::Start(const std::string& host, uint16_t port) {
    impl_->thread_ = std::thread([this, host, port]() {
        spdlog::info("ConfigServer listening on {}:{}", host, port);
        impl_->svr_.listen(host, port);
    });
}

void buf_connect_server::ConfigServer::Stop() {
    impl_->svr_.stop();
    if (impl_->thread_.joinable()) impl_->thread_.join();
}

void buf_connect_server::ConfigServer::RegisterDownloadToken(
        const std::string& token, const std::string& file_path, uint32_t ttl_seconds) {
    std::lock_guard<std::mutex> lock(impl_->token_mutex_);
    auto expires = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now()) + ttl_seconds;
    impl_->download_tokens_[token] = {file_path, expires};
}