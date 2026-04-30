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

    struct SessionConfig {
        uint32_t admin_conflict_timeout_seconds = 20;
        uint32_t snooze_duration_seconds        = 5;
        uint32_t max_concurrent_sessions        = 0;

        uint32_t call_token_renew_period        = 20;
        uint32_t grace_period_admin_seconds     = 15;
        uint32_t grace_period_engineer_seconds  = 15;
    };

    struct StreamingConfig
    {
        bool        compression_enabled          = false;
        std::string compression_algorithm        = "zstd";
    };

    struct LogConfig {
        std::string level        = "info";
        std::string log_file_name_prefix = "buf_connect_server"; // e.g. buf_connect_server_1.log
        bool        console      = true;
        uint32_t    max_size_mb  = 64;
        uint32_t    max_files    = 6;
    };

    struct MetricsConfig {
        bool        enabled = false;
        std::string metrics_log_path           = "/metrics";
        std::string metrics_log_file_prefix    = "metrics_session_from_"; // + server's start timestamp
    };

    struct ServerConfig {
        InterfaceConfig control_plane;
        InterfaceConfig data_plane;
        SessionConfig   session;
        StreamingConfig streaming;
        LogConfig       log;
        MetricsConfig   metrics;
        std::string     user_db_path = "users.db";
    };

}  // namespace buf_connect_server
