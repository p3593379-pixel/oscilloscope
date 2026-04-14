// FILE: buf_connect_server/src/http2/router.cpp
#include "buf_connect_server/http2/router.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

buf_connect_server::http2::Http2Router::Http2Router() = default;

void buf_connect_server::http2::Http2Router::Register(
        const std::string& path, RouteHandler handler) {
    routes_[path] = std::move(handler);
}

void buf_connect_server::http2::Http2Router::Dispatch(
        const connect::ParsedConnectRequest& req,
        connect::ConnectResponseWriter& writer) const {
    auto it = routes_.find(req.path);
    if (it == routes_.end()) {
        writer.SendHeaders(connect::kHttpNotFound, "application/json");
        writer.WriteError(std::string(connect::kCodeNotFound),
                          "RPC method not found: " + req.path);
        return;
    }
    spdlog::debug("Dispatching RPC: {}", req.path);
    it->second(req, writer);
}
