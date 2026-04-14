#include "buf_connect_server/services/session_handler.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include "buf_connect_server/connect/frame_codec.hpp"
#include "buf_connect_server.pb.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace buf_connect_server::services {

// ─── helpers ─────────────────────────────────────────────────────────────────

    static std::vector<uint8_t> ExtractUnaryBody(
            const connect::ParsedConnectRequest& req) {
        if (req.body.size() < 5) return req.body;
        auto decoded = connect::DecodeFrame(std::span<const uint8_t>(req.body));
        if (decoded.bytes_consumed > 0)
            return {decoded.payload.begin(), decoded.payload.end()};
        return req.body;
    }

// ─── constructor ─────────────────────────────────────────────────────────────

    SessionHandler::SessionHandler(session::SessionManager& mgr,
                                   const AuthConfig&        auth_config)
            : mgr_(mgr), auth_config_(auth_config) {
        stream_token_    = std::make_shared<auth::StreamToken>(auth_config_.jwt_secret);
        auth_middleware_ = std::make_shared<auth::AuthMiddleware>(
                std::make_shared<auth::JwtUtils>(auth_config_.jwt_secret));
    }

    std::string SessionHandler::ServicePath() const {
        return "/buf_connect_server.v2.SessionService";
    }

    void SessionHandler::RegisterRoutes(BufConnectServer& server) {
        server.RegisterControlRoute(
                "/buf_connect_server.v2.SessionService/WatchSessionEvents",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) {
                    HandleWatchSessionEvents(req, w);
                });
        server.RegisterControlRoute(
                "/buf_connect_server.v2.SessionService/GetStreamToken",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) {
                    HandleGetStreamToken(req, w);
                });
        server.RegisterControlRoute(
                "/buf_connect_server.v2.SessionService/ClaimActiveRole",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) {
                    HandleClaimActiveRole(req, w);
                });
        server.RegisterControlRoute(
                "/buf_connect_server.v2.SessionService/AdminConflictResponse",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) {
                    HandleAdminConflictResponse(req, w);
                });
        server.RegisterControlRoute(
                "/buf_connect_server.v2.SessionService/Heartbeat",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) {
                    HandleHeartbeat(req, w);
                });
    }

// ─── WatchSessionEvents ──────────────────────────────────────────────────────

    void SessionHandler::HandleWatchSessionEvents(
            const connect::ParsedConnectRequest& req,
            connect::ConnectResponseWriter& writer) {
        namespace c = connect;

        auto auth_it = req.headers.find("authorization");
        if (auth_it == req.headers.end()) {
            writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            writer.WriteError(std::string(c::kCodeUnauthenticated),
                              "missing Authorization header");
            return;
        }
        auto ctx = auth_middleware_->Authenticate(auth_it->second);
        if (!ctx) {
            writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            writer.WriteError(std::string(c::kCodeUnauthenticated),
                              "invalid or expired token");
            return;
        }

        const std::string& session_id = ctx->session_id;
        mgr_.Connect(session_id, ctx->user_id, ctx->role);

        writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeConnectProto));
        mgr_.Subscribe(session_id, &writer);

        uint32_t tick_count = 0;
        while (writer.IsClientConnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (++tick_count % 50 == 0)
                mgr_.UpdateHeartbeat(session_id);
        }

        mgr_.Unsubscribe(session_id);
        mgr_.Disconnect(session_id);
        writer.WriteEndOfStream();
        spdlog::info("WatchSessionEvents stream closed for connection '{}'",
                     session_id);
    }

// ─── GetStreamToken ───────────────────────────────────────────────────────────
// Option A: token carries only sub + exp; tier is negotiated on the data plane.

    void SessionHandler::HandleGetStreamToken(
            const connect::ParsedConnectRequest& req,
            connect::ConnectResponseWriter& w) {
        namespace c = connect;

        auto auth_it = req.headers.find("authorization");
        if (auth_it == req.headers.end()) {
            w.SendHeaders(c::kHttpUnauthorized, "application/json");
            w.WriteError(std::string(c::kCodeUnauthenticated),
                         "missing Authorization header");
            return;
        }
        auto ctx = auth_middleware_->Authenticate(auth_it->second);
        if (!ctx) {
            w.SendHeaders(c::kHttpUnauthorized, "application/json");
            w.WriteError(std::string(c::kCodeUnauthenticated),
                         "invalid or expired token");
            return;
        }

        auto now = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());
        auth::StreamTokenClaims claims;
        claims.sub  = ctx->user_id;
        claims.tier = "";   // tier-agnostic (Option A)
        claims.iat  = now;
        claims.exp  = now + auth_config_.stream_token_ttl_seconds;
        auto token  = stream_token_->Issue(claims);

        v2::GetStreamTokenResponse resp;
        resp.set_stream_token(token);
        resp.set_ttl_seconds(auth_config_.stream_token_ttl_seconds);

        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
        w.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                            std::span<const uint8_t>(out));
    }

// ─── ClaimActiveRole ─────────────────────────────────────────────────────────

    void SessionHandler::HandleClaimActiveRole(
            const connect::ParsedConnectRequest& req,
            connect::ConnectResponseWriter& w) {
        namespace c = connect;

        auto connection_id = req.headers.count("x-connection-id")
                             ? req.headers.at("x-connection-id") : "";
        auto result = mgr_.ClaimActiveRole(connection_id);

        v2::ClaimActiveRoleResponse resp;
        resp.set_granted(result.accepted);
        resp.set_reason(result.reason);
        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
        w.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                            std::span<const uint8_t>(out));
    }

// ─── AdminConflictResponse ────────────────────────────────────────────────────

    void SessionHandler::HandleAdminConflictResponse(
            const connect::ParsedConnectRequest& req,
            connect::ConnectResponseWriter& w) {
        namespace c = connect;

        auto body = ExtractUnaryBody(req);
        v2::AdminConflictResponseRequest conflict_req;
        if (!conflict_req.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            w.SendHeaders(c::kHttpBadRequest, "application/json");
            w.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
            return;
        }

        std::string choice;
        switch (conflict_req.choice()) {
            case v2::ADMIN_CONFLICT_CHOICE_GRANT:  choice = "GRANT";  break;
            case v2::ADMIN_CONFLICT_CHOICE_SNOOZE: choice = "SNOOZE"; break;
            default:                               choice = "KEEP";   break;
        }

        auto connection_id = req.headers.count("x-connection-id")
                             ? req.headers.at("x-connection-id") : "";
        mgr_.AdminConflictResponse(connection_id, choice);

        v2::ClaimActiveRoleResponse resp;
        resp.set_granted(true);
        resp.set_reason(choice);
        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
        w.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                            std::span<const uint8_t>(out));
    }

    void SessionHandler::HandleHeartbeat(
            const connect::ParsedConnectRequest& req,
            connect::ConnectResponseWriter& w) {
        namespace c = connect;
        auto auth_it = req.headers.find("authorization");
        if (auth_it == req.headers.end()) {
            w.SendHeaders(c::kHttpUnauthorized, "application/json");
            w.WriteError(std::string(c::kCodeUnauthenticated), "missing token");
            return;
        }
        auto ctx = auth_middleware_->Authenticate(auth_it->second);
        if (!ctx) {
            w.SendHeaders(c::kHttpUnauthorized, "application/json");
            w.WriteError(std::string(c::kCodeUnauthenticated), "invalid token");
            return;
        }
        mgr_.UpdateHeartbeat(ctx->session_id);

        v2::HeartbeatResponse resp;
        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
        w.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                            std::span<const uint8_t>(out));
    }

}  // namespace buf_connect_server::services
