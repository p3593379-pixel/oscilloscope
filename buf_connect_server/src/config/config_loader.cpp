#include "buf_connect_server/config/server_config.hpp"
#include "buf_connect_server/config/config_loader.hpp"

#include <fstream>
#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace buf_connect_server {
    using json = nlohmann::json;

    // -----------------------------------------------------------------------
    // TlsConfig
    // -----------------------------------------------------------------------
    static void from_json(const json& j, TlsConfig& t) {
        t.enabled     = j.value("enabled",     false);
        t.cert_path   = j.value("cert_path",   "");
        t.key_path    = j.value("key_path",    "");
        t.min_version = j.value("min_version", "TLSv1.2");
    }

    static void to_json(json& j, const TlsConfig& t) {
        j = {{"enabled",     t.enabled},
             {"cert_path",   t.cert_path},
             {"key_path",    t.key_path},
             {"min_version", t.min_version}};
    }

    // -----------------------------------------------------------------------
    // InterfaceConfig
    // -----------------------------------------------------------------------
    static void from_json(const json& j, InterfaceConfig& i) {
        i.bind_address = j.value("bind_address", "0.0.0.0");
        i.port         = j.value("port",         uint16_t{8080});
        i.enabled      = j.value("enabled",      true);
        if (j.contains("tls")) from_json(j["tls"], i.tls);
    }

    static void to_json(json& j, const InterfaceConfig& i) {
        json tls_j;
        to_json(tls_j, i.tls);
        j = {{"bind_address", i.bind_address},
             {"port",         i.port},
             {"enabled",      i.enabled},
             {"tls",          tls_j}};
    }

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------
    ServerConfig ConfigLoader::LoadFromFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) {
            spdlog::warn("Config file '{}' not found — using defaults", path);
            return ServerConfig{};
        }
        try {
            json j = json::parse(f);
            return FromJson(j.dump());
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse config file '{}': {}", path, e.what());
            return ServerConfig{};
        }
    }

    void ConfigLoader::SaveToFile(const ServerConfig& config, const std::string& path) {
        std::ofstream f(path);
        if (!f.is_open())
            throw std::runtime_error("Cannot open config file for writing: " + path);
        f << ToJson(config);
    }

    ServerConfig ConfigLoader::FromJson(const std::string& json_str) {
        auto j = json::parse(json_str);
        ServerConfig c;

        if (j.contains("control_plane")) from_json(j["control_plane"], c.control_plane);
        if (j.contains("data_plane"))    from_json(j["data_plane"],    c.data_plane);

        // Session ----------------------------------------------------------------
        if (j.contains("session")) {
            const auto& s = j["session"];
            c.session.admin_conflict_timeout_seconds =
                    s.value("admin_conflict_timeout_seconds", uint32_t{20});
            c.session.snooze_duration_seconds =
                    s.value("snooze_duration_seconds",        uint32_t{5});
            c.session.max_concurrent_sessions =
                    s.value("max_concurrent_sessions",        uint32_t{0});
            c.session.call_token_renew_period =
                    s.value("call_token_renew_period",        uint32_t{20});
            c.session.grace_period_admin_seconds =
                    s.value("grace_period_admin_seconds",     uint32_t{15});
            c.session.grace_period_engineer_seconds =
                    s.value("grace_period_engineer_seconds",  uint32_t{15});
        }

        // Streaming --------------------------------------------------------------
        if (j.contains("streaming")) {
            const auto& s = j["streaming"];
            c.streaming.compression_enabled   =
                    s.value("compression_enabled",   false);
            c.streaming.compression_algorithm  =
                    s.value("compression_algorithm", std::string{"zstd"});
        }

        // Log --------------------------------------------------------------------
        if (j.contains("log")) {
            const auto& l = j["log"];
            c.log.level                = l.value("level",                std::string{"info"});
            c.log.log_file_name_prefix = l.value("log_file_name_prefix", std::string{"buf_connect_server"});
            c.log.console              = l.value("console",              true);
            c.log.max_size_mb          = l.value("max_size_mb",          uint32_t{64});
            c.log.max_files            = l.value("max_files",            uint32_t{6});
        }

        // Metrics ----------------------------------------------------------------
        if (j.contains("metrics")) {
            const auto& m = j["metrics"];
            c.metrics.enabled                 = m.value("enabled",                 false);
            c.metrics.metrics_log_path        = m.value("metrics_log_path",        std::string{"/metrics"});
            c.metrics.metrics_log_file_prefix = m.value("metrics_log_file_prefix", std::string{"metrics_session_from_"});
        }

        c.user_db_path = j.value("user_db_path", std::string{"users.db"});
        return c;
    }

    std::string ConfigLoader::ToJson(const ServerConfig& c) {
        json cp_j, dp_j;
        to_json(cp_j, c.control_plane);
        to_json(dp_j, c.data_plane);

        json j = {
                {"control_plane", cp_j},
                {"data_plane",    dp_j},
                {"session", {
                                          {"admin_conflict_timeout_seconds", c.session.admin_conflict_timeout_seconds},
                                          {"snooze_duration_seconds",        c.session.snooze_duration_seconds},
                                          {"max_concurrent_sessions",        c.session.max_concurrent_sessions},
                                          {"call_token_renew_period",        c.session.call_token_renew_period},
                                          {"grace_period_admin_seconds",     c.session.grace_period_admin_seconds},
                                          {"grace_period_engineer_seconds",  c.session.grace_period_engineer_seconds}
                                  }},
                {"streaming", {
                                          {"compression_enabled",   c.streaming.compression_enabled},
                                          {"compression_algorithm", c.streaming.compression_algorithm}
                                  }},
                {"log", {
                                          {"level",                c.log.level},
                                          {"log_file_name_prefix", c.log.log_file_name_prefix},
                                          {"console",              c.log.console},
                                          {"max_size_mb",          c.log.max_size_mb},
                                          {"max_files",            c.log.max_files}
                                  }},
                {"metrics", {
                                          {"enabled",                 c.metrics.enabled},
                                          {"metrics_log_path",        c.metrics.metrics_log_path},
                                          {"metrics_log_file_prefix", c.metrics.metrics_log_file_prefix}
                                  }},
                {"user_db_path", c.user_db_path}
        };
        return j.dump(2);
    }

}  // namespace buf_connect_server