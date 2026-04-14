// FILE: buf_connect_server/src/connect/request.cpp
#include "buf_connect_server/connect/request.hpp"
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
