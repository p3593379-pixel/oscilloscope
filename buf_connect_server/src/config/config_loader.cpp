#include "buf_connect_server/config/server_config.hpp"
#include "buf_connect_server/config/config_loader.hpp"

#include <fstream>
#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace buf_connect_server {
    using json = nlohmann::json;
// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

    static void from_json(const json& j, TlsConfig& t) {
        t.enabled     = j.value("enabled",     false);
        t.cert_path   = j.value("cert_path",   "");
        t.key_path    = j.value("key_path",    "");
        t.min_version = j.value("min_version", "TLSv1.2");
    }

    static void to_json(json& j, const TlsConfig& t) {
        j = {{"enabled", t.enabled}, {"cert_path", t.cert_path},
             {"key_path", t.key_path}, {"min_version", t.min_version}};
    }

    static void from_json(const json& j, InterfaceConfig& i) {
        i.bind_address = j.value("bind_address", "0.0.0.0");
        i.port         = j.value("port",         uint16_t{8080});
        i.enabled      = j.value("enabled",      true);
        if (j.contains("tls")) from_json(j["tls"], i.tls);
    }

    static void to_json(json& j, const InterfaceConfig& i) {
        json tls_j; to_json(tls_j, i.tls);
        j = {{"bind_address", i.bind_address}, {"port", i.port},
             {"enabled", i.enabled}, {"tls", tls_j}};
    }

    ServerConfig ConfigLoader::LoadFromFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) {
            return ServerConfig{};  // Return defaults if file not found
        }
        json j = json::parse(f);
        return FromJson(j.dump());
    }

    void ConfigLoader::SaveToFile(const ServerConfig& config, const std::string& path) {
        std::ofstream f(path);
        if (!f.is_open()) throw std::runtime_error("Cannot open config file for writing: " + path);
        f << ToJson(config);
    }

    ServerConfig ConfigLoader::FromJson(const std::string& json_str) {
        auto j = json::parse(json_str);
        ServerConfig c;
        if (j.contains("control_plane")) from_json(j["control_plane"], c.control_plane);
        if (j.contains("data_plane"))    from_json(j["data_plane"],    c.data_plane);
        c.single_interface_mode = j.value("single_interface_mode", false);

        if (j.contains("auth")) {
            auto& a = j["auth"];
            c.auth.jwt_secret                  = a.value("jwt_secret", "");
            c.auth.access_token_ttl_seconds    = a.value("access_token_ttl_seconds",  uint32_t{900});
            c.auth.refresh_token_ttl_seconds   = a.value("refresh_token_ttl_seconds", uint32_t{604800});
            c.auth.stream_token_ttl_seconds    = a.value("stream_token_ttl_seconds",  uint32_t{30});
        }
        if (j.contains("session")) {
            auto& s = j["session"];
            c.session.admin_conflict_timeout_seconds = s.value("admin_conflict_timeout_seconds", uint32_t{20});
            c.session.snooze_duration_seconds        = s.value("snooze_duration_seconds",        uint32_t{600});
            c.session.max_concurrent_sessions        = s.value("max_concurrent_sessions",        uint32_t{0});
        }
        if (j.contains("streaming")) {
            auto& s = j["streaming"];
            c.streaming.max_clients                = s.value("max_clients",                uint32_t{0});
            c.streaming.frame_size_bytes           = s.value("frame_size_bytes",           uint32_t{65536});
            c.streaming.compression_enabled        = s.value("compression_enabled",        false);
            c.streaming.compression_algorithm      = s.value("compression_algorithm",      std::string{"zstd"});
            c.streaming.compression_threshold_bytes= s.value("compression_threshold_bytes",uint32_t{1024});
            c.streaming.backpressure_policy        = s.value("backpressure_policy",        std::string{"disconnect"});
        }
        if (j.contains("log")) {
            auto& l = j["log"];
            c.log.level       = l.value("level",       std::string{"info"});
            c.log.destination = l.value("destination", std::string{"stdout"});
            c.log.file_path   = l.value("file_path",   std::string{"buf_connect_server.log"});
            c.log.max_size_mb = l.value("max_size_mb", uint32_t{64});
            c.log.max_files   = l.value("max_files",   uint32_t{5});
            c.log.access_log  = l.value("access_log",  true);
        }
        if (j.contains("metrics")) {
            c.metrics.enabled = j["metrics"].value("enabled", false);
            c.metrics.path    = j["metrics"].value("path",    std::string{"/metrics"});
        }
        return c;
    }

    std::string ConfigLoader::ToJson(const ServerConfig& c)
    {
        json cp, dp;
        to_json(cp, c.control_plane);
        to_json(dp, c.data_plane);
        json j = {
                {"control_plane",         cp},
                {"data_plane",            dp},
                {"single_interface_mode", c.single_interface_mode},
                {"auth",                  {
                                                  {"access_token_ttl_seconds",       c.auth.access_token_ttl_seconds},
                                                                                                         {"refresh_token_ttl_seconds", c.auth.refresh_token_ttl_seconds},
                                                  {"stream_token_ttl_seconds", c.auth.stream_token_ttl_seconds}
                                                  // jwt_secret intentionally omitted from JSON output
                                          }},
                {"session",               {
                                                  {"admin_conflict_timeout_seconds", c.session.admin_conflict_timeout_seconds},
                                                                                                         {"snooze_duration_seconds",   c.session.snooze_duration_seconds},
                                                  {"max_concurrent_sessions",  c.session.max_concurrent_sessions}
                                          }},
                {"streaming",             {
                                                  {"max_clients",                    c.streaming.max_clients},
                                                                                                         {"frame_size_bytes",          c.streaming.frame_size_bytes},
                                                  {"compression_enabled",      c.streaming.compression_enabled},
                                                                                                 {"compression_algorithm", c.streaming.compression_algorithm},
                                                  {"compression_threshold_bytes", c.streaming.compression_threshold_bytes},
                                                                                                    {"backpressure_policy", c.streaming.backpressure_policy}
                                          }},
                {"log",                   {
                                                  {"level",                          c.log.level},       {"destination",               c.log.destination},
                                                  {"file_path",                c.log.file_path}, {"max_size_mb",           c.log.max_size_mb},
                                                  {"max_files",                   c.log.max_files}, {"access_log",          c.log.access_log}
                                          }},
                {"metrics",               {       {"enabled",                        c.metrics.enabled}, {"path",                      c.metrics.path}}}
        };
        return j.dump(2);
    }

    static void LoadTlsConfig(const nlohmann::json& obj, TlsConfig& tls) {
        tls.enabled     = obj.value("enabled",     tls.enabled);
        tls.cert_path   = obj.value("cert_path",   tls.cert_path);
        tls.key_path    = obj.value("key_path",    tls.key_path);
        tls.min_version = obj.value("min_version", tls.min_version);
    }

    static void LoadInterfaceConfig(const nlohmann::json& obj, InterfaceConfig& iface) {
        iface.bind_address = obj.value("bind_address", iface.bind_address);
        iface.port         = obj.value("port",         iface.port);
        iface.enabled      = obj.value("enabled",      iface.enabled);
        if (obj.contains("tls") && obj["tls"].is_object()) {
            LoadTlsConfig(obj["tls"], iface.tls);
        }
    }

    static void LoadAuthConfig(const nlohmann::json& obj, AuthConfig& auth)
    {
        auth.jwt_secret = obj.value("jwt_secret", auth.jwt_secret);

        // Legacy fields — read as before so existing configs continue to work
        if (obj.contains("access_token_ttl_seconds")) {
            spdlog::warn("[config] 'access_token_ttl_seconds' is deprecated; "
                         "prefer 'call_token_ttl_seconds'");
            auth.access_token_ttl_seconds = obj["access_token_ttl_seconds"].get<uint32_t>();
        }
        if (obj.contains("refresh_token_ttl_seconds")) {
            spdlog::warn("[config] 'refresh_token_ttl_seconds' is deprecated; "
                         "prefer 'session_ticket_ttl_seconds'");
            auth.refresh_token_ttl_seconds = obj["refresh_token_ttl_seconds"].get<uint32_t>();
        }
        auth.stream_token_ttl_seconds = obj.value("stream_token_ttl_seconds", auth.stream_token_ttl_seconds);

        // New canonical fields
        auth.call_token_ttl_seconds     = obj.value("call_token_ttl_seconds", auth.call_token_ttl_seconds);
        auth.session_ticket_ttl_seconds = obj.value("session_ticket_ttl_seconds", auth.session_ticket_ttl_seconds);
    }

    static void LoadSessionConfig(const nlohmann::json& obj, SessionConfig& session) {
        // Existing fields
        session.admin_conflict_timeout_seconds =
                obj.value("admin_conflict_timeout_seconds",
                          session.admin_conflict_timeout_seconds);
        session.snooze_duration_seconds =
                obj.value("snooze_duration_seconds", session.snooze_duration_seconds);
        session.max_concurrent_sessions =
                obj.value("max_concurrent_sessions", session.max_concurrent_sessions);

        // New fields
        session.heartbeat_interval_seconds =
                obj.value("heartbeat_interval_seconds", session.heartbeat_interval_seconds);
        session.grace_period_admin_seconds =
                obj.value("grace_period_admin_seconds", session.grace_period_admin_seconds);
        session.grace_period_engineer_seconds =
                obj.value("grace_period_engineer_seconds",
                          session.grace_period_engineer_seconds);
    }

    static void LoadStreamingConfig(const nlohmann::json& obj, StreamingConfig& streaming) {
        streaming.data_plane_tls_enabled = obj.value("data_plane_tls_enabled", streaming.data_plane_tls_enabled);
        streaming.max_clients = obj.value("max_clients", streaming.max_clients);
        streaming.frame_size_bytes       = obj.value("frame_size_bytes",       streaming.frame_size_bytes);
        streaming.compression_enabled     = obj.value("compression_enabled",     streaming.compression_enabled);
        streaming.compression_algorithm     = obj.value("compression_algorithm",     streaming.compression_algorithm);
        streaming.compression_threshold_bytes     = obj.value("compression_threshold_bytes",     streaming.compression_threshold_bytes);
        streaming.backpressure_policy     = obj.value("backpressure_policy",     streaming.backpressure_policy);
    }

    static void LoadLogConfig(const nlohmann::json& obj, LogConfig& log) {
        log.level      = obj.value("level",      log.level);
        log.destination = obj.value("destination", log.destination);
        log.file_path  = obj.value("file_path",  log.file_path);
        log.console    = obj.value("console",    log.console);
        log.max_size_mb = obj.value("max_size_mb", log.max_size_mb);
        log.max_files = obj.value("max_files", log.max_files);
        log.access_log = obj.value("access_log", log.access_log);
    }

    static void LoadMetricsConfig(const nlohmann::json& obj, MetricsConfig& metrics) {
        metrics.enabled      = obj.value("enabled",      metrics.enabled);
        metrics.path         = obj.value("path",         metrics.path);
    }

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

    ServerConfig LoadServerConfig(const std::string& path) {
        ServerConfig config;

        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::warn("[config] Could not open config file '{}'; using defaults", path);
            return config;
        }

        nlohmann::json root;
        try {
            file >> root;
        } catch (const nlohmann::json::parse_error& e) {
            spdlog::error("[config] JSON parse error in '{}': {}", path, e.what());
            return config;
        }

        if (root.contains("control_plane") && root["control_plane"].is_object()) {
            LoadInterfaceConfig(root["control_plane"], config.control_plane);
        }
        if (root.contains("data_plane") && root["data_plane"].is_object()) {
            LoadInterfaceConfig(root["data_plane"], config.data_plane);
        }

        config.single_interface_mode =
                root.value("single_interface_mode", config.single_interface_mode);

        if (root.contains("auth") && root["auth"].is_object()) {
            LoadAuthConfig(root["auth"], config.auth);
        }
        if (root.contains("session") && root["session"].is_object()) {
            LoadSessionConfig(root["session"], config.session);
        }
        if (root.contains("streaming") && root["streaming"].is_object()) {
            LoadStreamingConfig(root["streaming"], config.streaming);
        }
        if (root.contains("log") && root["log"].is_object()) {
            LoadLogConfig(root["log"], config.log);
        }
        if (root.contains("metrics") && root["metrics"].is_object()) {
            LoadMetricsConfig(root["metrics"], config.metrics);
        }

        config.user_db_path       = root.value("user_db_path",       config.user_db_path);
        config.config_server_path = root.value("config_server_path", config.config_server_path);

        return config;
    }

}  // namespace buf_connect_server
