// FILE: oscilloscope_backend/src/services/archive_service_impl.hpp
#ifndef OSCILLOSCOPE_BACKEND_SERVICES_ARCHIVE_SERVICE_IMPL_HPP
#define OSCILLOSCOPE_BACKEND_SERVICES_ARCHIVE_SERVICE_IMPL_HPP

#include "buf_connect_server/server.hpp"
#include "buf_connect_server/config/server_config.hpp"
#include "buf_connect_server/auth/stream_token.hpp"

class ArchiveServiceImpl : public buf_connect_server::ServiceHandlerBase {
public:
    explicit ArchiveServiceImpl(const buf_connect_server::AuthConfig& auth_config);
    std::string ServicePath() const override;
    void RegisterRoutes(buf_connect_server::BufConnectServer& server) override;

private:
    buf_connect_server::AuthConfig                auth_config_;
    std::shared_ptr<buf_connect_server::auth::StreamToken> stream_token_;

    void HandleRequestDownload(
            const buf_connect_server::connect::ParsedConnectRequest&,
            buf_connect_server::connect::ConnectResponseWriter&,
            buf_connect_server::BufConnectServer& server);
};

#endif
