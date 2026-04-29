// FILE: buf_connect_server/src/auth/jwt.cpp
#include "buf_connect_server/auth/jwt.hpp"
#include "spdlog/spdlog.h"
#include <jwt-cpp/jwt.h>
#include <chrono>
#include <sstream>

buf_connect_server::auth::JwtIssuer::JwtIssuer(std::string secret)
        : secret_(std::move(secret)) {}


std::string buf_connect_server::auth::JwtIssuer::Issue(const JwtClaims &claims) const
{
//    SPDLOG_INFO("Token issued. UUID: {}; Role: {}; Type: {};", claims.session_uuid, claims.role, claims.type);
    return jwt::create()
            .set_issuer(kIssuer)
            .set_subject(claims.sub)
            .set_issued_at(claims.issued_at)
            .set_expires_at(claims.expires_at)
            .set_payload_claim("role",         jwt::claim(claims.role))
            .set_payload_claim("session_uuid", jwt::claim(claims.session_uuid))
            .set_payload_claim("type",         jwt::claim(claims.type))
            .sign(jwt::algorithm::hs256{secret_});
}

using namespace std::chrono_literals;

std::optional<buf_connect_server::auth::JwtClaims>
buf_connect_server::auth::JwtIssuer::Verify(const std::string &token) const
{
    try {
        auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{secret_})
                .leeway(std::chrono::seconds(5).count());

        auto decoded = jwt::decode(token);
        verifier.verify(decoded);

        JwtClaims c;
        c.sub          = decoded.get_subject();
        c.role         = decoded.get_payload_claim("role").as_string();
        c.session_uuid   = decoded.get_payload_claim("session_uuid").as_string();
        c.type         = decoded.get_payload_claim("type").as_string();
        c.issued_at    = decoded.get_issued_at();
        c.expires_at   = decoded.get_expires_at();
//        SPDLOG_INFO("Token validated. UUID: {}; Role: {}; Type: {};", c.session_uuid, c.role, c.type);
        return c;
    } catch (...) {
        return std::nullopt;
    }
}


[[nodiscard]] std::string buf_connect_server::auth::BuildSessionTicketCookie(const std::string& token)
{
    std::ostringstream oss;
    oss << "session_ticket=" << token
        << "; HttpOnly; Secure; SameSite=Strict"
        << "; Path=/buf_connect_server.v2.AuthService/RenewCallToken";
    return oss.str();
}
