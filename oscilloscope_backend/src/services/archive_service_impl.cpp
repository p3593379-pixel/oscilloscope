// FILE: oscilloscope_backend/src/services/archive_service_impl.cpp
#include "services/archive_service_impl.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include "buf_connect_server/config/config_server.hpp"
#include "oscilloscope_interface.pb.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <filesystem>
#include <random>
#include <sstream>
#include <iomanip>

ArchiveServiceImpl::ArchiveServiceImpl(const std::string & _jwt_secret)
{
    stream_token_ = std::make_shared<buf_connect_server::auth::StreamToken>(_jwt_secret);
}

std::string ArchiveServiceImpl::ServicePath() const {
    return "/oscilloscope_interface.v2.ArchiveService";
}

void ArchiveServiceImpl::RegisterRoutes(buf_connect_server::BufConnectServer& server) {
    namespace c = buf_connect_server::connect;
    server.RegisterControlRoute(
            "/oscilloscope_interface.v2.ArchiveService/RequestDownload",
            [this, &server](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleRequestDownload(req, w, server);
            });
}

void ArchiveServiceImpl::HandleRequestDownload(
        const buf_connect_server::connect::ParsedConnectRequest& req,
        buf_connect_server::connect::ConnectResponseWriter& writer,
        buf_connect_server::BufConnectServer& /*server*/) {
    namespace c = buf_connect_server::connect;

    oscilloscope_interface::v2::RequestDownloadRequest dl_req;
    if (!dl_req.ParseFromArray(req.body.data(), static_cast<int>(req.body.size()))) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
        return;
    }

    const std::string& file_path = dl_req.file_path();
    if (!std::filesystem::exists(file_path)) {
        writer.SendHeaders(c::kHttpNotFound, "application/json");
        writer.WriteError(std::string(c::kCodeNotFound), "file not found");
        return;
    }

    // Issue 60-second stream token
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    buf_connect_server::auth::StreamTokenClaims claims;
    claims.sub  = "archive";
    claims.tier = "full";
    claims.iat  = now;
    claims.exp  = now + 60;
    auto token = stream_token_->Issue(claims);

    uint64_t file_size = std::filesystem::file_size(file_path);

    // Build download URL
    const auto& cp = req.headers.find("host");
    std::string host = cp != req.headers.end() ? cp->second : "localhost:8081";
    std::string url = "http://" + host + "/api/download/" + token;

    oscilloscope_interface::v2::RequestDownloadResponse resp;
    resp.set_download_url(url);
    resp.set_token(token);
    resp.set_ttl_seconds(60);
    resp.set_file_size(file_size);

    std::vector<uint8_t> out(resp.ByteSizeLong());
    resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeProto));
    writer.WriteUnary(std::span<const uint8_t>(out));
}
