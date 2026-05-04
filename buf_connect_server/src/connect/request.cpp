// FILE: buf_connect_server/src/connect/request.cpp
#include "buf_connect_server/connect/request.hpp"
#include "buf_connect_server/connect/frame_codec.hpp"
#include <string_view>

std::string buf_connect_server::connect::ExtractBearerToken(const std::string& authorization_header)
{
    std::string_view sv(authorization_header);
    constexpr std::string_view prefix = "Bearer ";
    if (sv.substr(0, prefix.size()) == prefix) {
        return std::string(sv.substr(prefix.size()));
    }
    return {};
}

std::string buf_connect_server::connect::ExtractAuthorizationBearer(const buf_connect_server::connect::ParsedConnectRequest& req)
{
    namespace c = connect;
    auto auth_it = req.headers.find("authorization");
    if (auth_it == req.headers.end() || auth_it->second.rfind("Bearer ", 0) != 0) {
        return {};
    }
    return auth_it->second.substr(7); // strip "Bearer "
}

std::vector<uint8_t> buf_connect_server::connect::ExtractUnaryBody(const connect::ParsedConnectRequest& req)
{
    if (req.body.size() < 5)
        return req.body;
    auto decoded = buf_connect_server::connect::DecodeFrame(std::span<const uint8_t>(req.body));
    if (decoded.bytes_consumed > 0)
        return {decoded.payload.begin(), decoded.payload.end()};
    return req.body;
}
