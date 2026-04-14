#include "buf_connect_server/services/auth_handler.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include "buf_connect_server/connect/frame_codec.hpp"
#include "buf_connect_server.pb.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <uuid/uuid.h>

namespace buf_connect_server::services {

// ─── helpers ─────────────────────────────────────────────────────────────────

    static std::string GenerateUuid() {
        uuid_t uuid;
        uuid_generate_random(uuid);
        char buf[37];
        uuid_unparse_lower(uuid, buf);
        return {buf};
    }

    static std::vector<uint8_t> ExtractUnaryBody(const connect::ParsedConnectRequest& req)
    {
        if (req.body.size() < 5)
            return req.body;
        auto decoded = connect::DecodeFrame(std::span<const uint8_t>(req.body));
        if (decoded.bytes_consumed > 0)
            return {decoded.payload.begin(), decoded.payload.end()};
        return req.body;
    }

// ─── constructor ─────────────────────────────────────────────────────────────

    AuthHandler::AuthHandler(auth::UserStore&  user_store,
                             const AuthConfig& auth_config)
            : user_store_(user_store), auth_config_(auth_config) {
        jwt_utils_ = std::make_shared<auth::JwtUtils>(auth_config_.jwt_secret);
    }

    std::string AuthHandler::ServicePath() const {
        return "/buf_connect_server.v2.AuthService";
    }

    void AuthHandler::RegisterRoutes(BufConnectServer& server) {
        server.RegisterControlRoute(
                "/buf_connect_server.v2.AuthService/Login",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) { HandleLogin(req, w); });
        server.RegisterControlRoute(
                "/buf_connect_server.v2.AuthService/Refresh",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) { HandleRefresh(req, w); });
    }

// ─── Login ───────────────────────────────────────────────────────────────────

    void AuthHandler::HandleLogin(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& writer)
    {
        namespace c = connect;

        auto body = ExtractUnaryBody(req);
        v2::LoginRequest login_req;
        if (!login_req.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            writer.SendHeaders(c::kHttpBadRequest, "application/json");
            writer.WriteError(std::string(c::kCodeInvalidArgument),
                              "failed to parse LoginRequest");
            return;
        }

        auto user = user_store_.FindByUsername(login_req.username());
        if (!user || !user_store_.VerifyPassword(user->id, login_req.password())) {
            writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            writer.WriteError(std::string(c::kCodeUnauthenticated), "invalid credentials");
            return;
        }

        user_store_.UpdateLastLogin(user->id);

        const std::string session_id = GenerateUuid();
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        auth::JwtClaims access_claims;
        std::string initial_mode = (user->role == "admin") ? "active_admin" : "active";
        access_claims.sub          = user->id;
        access_claims.role         = user->role;
        access_claims.session_mode = initial_mode;
        access_claims.session_id  = session_id;
        access_claims.type         = "access";
        access_claims.iat          = now;
        access_claims.exp          = now + auth_config_.access_token_ttl_seconds;

        auth::JwtClaims refresh_claims = access_claims;
        refresh_claims.type = "refresh";
        refresh_claims.exp  = now + auth_config_.refresh_token_ttl_seconds;

        auto access_token  = jwt_utils_->IssueToken(access_claims);
        auto refresh_token = jwt_utils_->IssueToken(refresh_claims);

        v2::LoginResponse resp;
        resp.set_access_token(access_token);
        resp.set_role(user->role == "admin"
                      ? v2::USER_ROLE_ADMIN
                      : v2::USER_ROLE_ENGINEER);
        resp.set_session_mode(user->role == "admin"
                              ? v2::SESSION_MODE_ACTIVE_ADMIN
                              : v2::SESSION_MODE_ACTIVE);
        resp.set_session_id(session_id);

        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));

        std::string cookie = "refresh_token=" + refresh_token
                             + "; Path=/buf_connect_server.v2.AuthService/Refresh"
                             + "; HttpOnly; SameSite=Strict; Max-Age="
                             + std::to_string(auth_config_.refresh_token_ttl_seconds);
        writer.AddHeader("set-cookie", cookie);
        writer.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                                 std::span<const uint8_t>(out));
        spdlog::info("Login success for user '{}'", login_req.username());
    }

// ─── Refresh ─────────────────────────────────────────────────────────────────

    void AuthHandler::HandleRefresh(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& writer)
    {
        namespace c = connect;

        auto cookie_it = req.headers.find("cookie");
        if (cookie_it == req.headers.end()) {
            writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            writer.WriteError(std::string(c::kCodeUnauthenticated), "no refresh token cookie");
            return;
        }

        std::string refresh_token;
        const std::string& cookies = cookie_it->second;
        auto pos = cookies.find("refresh_token=");
        if (pos != std::string::npos) {
            pos += 14;
            auto end = cookies.find(';', pos);
            refresh_token = cookies.substr(pos, end == std::string::npos ? end : end - pos);
        }
        if (refresh_token.empty()) {
            writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            writer.WriteError(std::string(c::kCodeUnauthenticated), "no refresh token cookie");
            return;
        }

        auto claims = jwt_utils_->ValidateToken(refresh_token);
        if (!claims || claims->type != "refresh") {
            writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            writer.WriteError(std::string(c::kCodeUnauthenticated), "invalid or expired refresh token");
            return;
        }

        auto user = user_store_.FindById(claims->sub);
        if (!user) {
            writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            writer.WriteError(std::string(c::kCodeUnauthenticated), "user not found");
            return;
        }

        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        auth::JwtClaims new_access;
        new_access.sub          = user->id;
        new_access.role         = user->role;
        new_access.session_mode = claims->session_mode;
        new_access.session_id  = claims->session_id;
        new_access.type         = "access";
        new_access.iat          = now;
        new_access.exp          = now + auth_config_.access_token_ttl_seconds;

        auth::JwtClaims new_refresh = new_access;
        new_refresh.type = "refresh";
        new_refresh.exp  = now + auth_config_.refresh_token_ttl_seconds;

        auto new_access_token  = jwt_utils_->IssueToken(new_access);
        auto new_refresh_token = jwt_utils_->IssueToken(new_refresh);

        v2::RefreshResponse resp;
        resp.set_access_token(new_access_token);
        resp.set_role(user->role == "admin" ? v2::USER_ROLE_ADMIN : v2::USER_ROLE_ENGINEER);
        v2::SessionMode mode = v2::SESSION_MODE_OBSERVER;
        if      (claims->session_mode == "active_admin") mode = v2::SESSION_MODE_ACTIVE_ADMIN;
        else if (claims->session_mode == "active")       mode = v2::SESSION_MODE_ACTIVE;
        else if (claims->session_mode == "on_service")   mode = v2::SESSION_MODE_ON_SERVICE;
        resp.set_session_mode(mode);
        resp.set_session_id(claims->session_id);
        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));

        std::string cookie = "refresh_token=" + new_refresh_token
                             + "; Path=/buf_connect_server.v2.AuthService/Refresh"
                             + "; HttpOnly; SameSite=Strict; Max-Age="
                             + std::to_string(auth_config_.refresh_token_ttl_seconds);
        writer.AddHeader("set-cookie", cookie);
        writer.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                                 std::span<const uint8_t>(out));
        spdlog::info("Token refreshed for user '{}'", user->username);
    }

}  // namespace buf_connect_server::services
