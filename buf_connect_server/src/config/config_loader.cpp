// FILE: buf_connect_server/src/config/config_loader.cpp
#include "buf_connect_server/config/config_loader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;
using namespace buf_connect_server;

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

std::string ConfigLoader::ToJson(const ServerConfig& c) {
    json cp, dp; to_json(cp, c.control_plane); to_json(dp, c.data_plane);
    json j = {
            {"control_plane", cp}, {"data_plane", dp},
            {"single_interface_mode", c.single_interface_mode},
            {"auth", {
                                      {"access_token_ttl_seconds",  c.auth.access_token_ttl_seconds},
                                                              {"refresh_token_ttl_seconds", c.auth.refresh_token_ttl_seconds},
                                      {"stream_token_ttl_seconds",  c.auth.stream_token_ttl_seconds}
                                      // jwt_secret intentionally omitted from JSON output
                              }},
            {"session", {
                                      {"admin_conflict_timeout_seconds", c.session.admin_conflict_timeout_seconds},
                                                              {"snooze_duration_seconds",        c.session.snooze_duration_seconds},
                                      {"max_concurrent_sessions",        c.session.max_concurrent_sessions}
                              }},
            {"streaming", {
                                      {"max_clients",                 c.streaming.max_clients},
                                                              {"frame_size_bytes",            c.streaming.frame_size_bytes},
                                      {"compression_enabled",         c.streaming.compression_enabled},
                                                                      {"compression_algorithm",       c.streaming.compression_algorithm},
                                      {"compression_threshold_bytes", c.streaming.compression_threshold_bytes},
                                                                      {"backpressure_policy",         c.streaming.backpressure_policy}
                              }},
            {"log", {
                                      {"level", c.log.level}, {"destination", c.log.destination},
                                      {"file_path", c.log.file_path}, {"max_size_mb", c.log.max_size_mb},
                                      {"max_files", c.log.max_files}, {"access_log", c.log.access_log}
                              }},
            {"metrics", {{"enabled", c.metrics.enabled}, {"path", c.metrics.path}}}
    };
    return j.dump(2);
}
