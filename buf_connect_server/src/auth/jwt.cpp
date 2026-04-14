// FILE: buf_connect_server/src/auth/jwt.cpp
#include "buf_connect_server/auth/jwt.hpp"
#include "spdlog/spdlog.h"
#include <jwt-cpp/jwt.h>
#include <chrono>

buf_connect_server::auth::JwtUtils::JwtUtils(std::string secret, std::string issuer)
        : secret_(std::move(secret)), issuer_(std::move(issuer)) {}


std::string buf_connect_server::auth::JwtUtils::IssueToken(const JwtClaims& claims) const
{
    SPDLOG_INFO("Token issued. Role: {}; Type: {};", claims.role, claims.type);
    return jwt::create()
            .set_issuer(issuer_)
            .set_subject(claims.sub)
            .set_issued_at(std::chrono::system_clock::from_time_t(claims.iat))
            .set_expires_at(std::chrono::system_clock::from_time_t(claims.exp))
            .set_payload_claim("role",         jwt::claim(claims.role))
            .set_payload_claim("session_mode", jwt::claim(claims.session_mode))
            .set_payload_claim("session_id", jwt::claim(claims.session_id))
            .set_payload_claim("type",         jwt::claim(claims.type))
            .sign(jwt::algorithm::hs256{secret_});
}

using namespace std::chrono_literals;

std::optional<buf_connect_server::auth::JwtClaims>
buf_connect_server::auth::JwtUtils::ValidateToken(const std::string& token) const {
    try {
        auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{secret_})
                .with_issuer(issuer_)
                .leeway(std::chrono::seconds(0).count());

        auto decoded = jwt::decode(token);
        verifier.verify(decoded);

        JwtClaims c;
        c.sub          = decoded.get_subject();
        c.role         = decoded.get_payload_claim("role").as_string();
        c.session_mode = decoded.get_payload_claim("session_mode").as_string();
        c.session_id = decoded.get_payload_claim("session_id").as_string();
        c.type         = decoded.get_payload_claim("type").as_string();
        c.iat          = std::chrono::system_clock::to_time_t(decoded.get_issued_at());
        c.exp          = std::chrono::system_clock::to_time_t(decoded.get_expires_at());
        SPDLOG_INFO("Token validated. Role: {}; Type: {};", c.role, c.type);
        return c;
    } catch (...) {
        return std::nullopt;
    }
}
