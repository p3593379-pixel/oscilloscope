#pragma once

#include <cstdint>
#include <string>

namespace buf_connect_server {

    struct TlsConfig {
        bool        enabled     = false;
        std::string cert_path;
        std::string key_path;
        std::string min_version = "TLSv1.2";
    };

    struct InterfaceConfig {
        std::string bind_address = "0.0.0.0";
        uint16_t    port         = 8080;
        bool        enabled      = true;
        TlsConfig   tls;
    };

    struct AuthConfig {
        std::string jwt_secret;

        // Legacy field names — kept for backward compatibility with existing config loaders
        uint32_t    access_token_ttl_seconds  = 5;
        uint32_t    refresh_token_ttl_seconds = 604800;
        uint32_t    stream_token_ttl_seconds  = 30;

        // New canonical field names
        uint32_t    call_token_ttl_seconds     = 5;
        uint32_t    session_ticket_ttl_seconds = 86400;
    };

    struct SessionConfig {
        // Existing fields — preserved as-is
        uint32_t admin_conflict_timeout_seconds = 20;
        uint32_t snooze_duration_seconds        = 5;
        uint32_t max_concurrent_sessions        = 0;

        // New fields
        uint32_t heartbeat_interval_seconds     = 20;
        uint32_t grace_period_admin_seconds     = 15;
        uint32_t grace_period_engineer_seconds  = 90;
    };

    struct StreamingConfig {
        bool        data_plane_tls_enabled       = false;
        uint32_t    max_clients                  = 0;
        uint32_t    frame_size_bytes             = 65536;
        bool        compression_enabled          = false;
        std::string compression_algorithm        = "zstd";
        uint32_t    compression_threshold_bytes  = 1024;
        std::string backpressure_policy          = "disconnect";
    };

    struct LogConfig {
        std::string level        = "info";
        std::string destination  = "console";
        std::string file_path    = "buf_connect_server.log";
        bool        console      = true;
        uint32_t    max_size_mb  = 64;
        uint32_t    max_files    = 6;
        bool        access_log   = true;
    };

    struct MetricsConfig {
        bool        enabled = false;
        std::string path    = "/metrics";
    };

    struct ServerConfig {
        InterfaceConfig control_plane;
        InterfaceConfig data_plane;
        bool            single_interface_mode = false;
        AuthConfig      auth;
        SessionConfig   session;
        StreamingConfig streaming;
        LogConfig       log;
        MetricsConfig   metrics;
        std::string     user_db_path       = "users.db";
        std::string     config_server_path = "/api/config";
    };

}  // namespace buf_connect_server
