#include "buf_connect_server/services/session_handler.hpp"

#include <spdlog/spdlog.h>

// Library headers
#include "buf_connect_server/auth/jwt.hpp"
#include "buf_connect_server/connect/request.hpp"
#include "buf_connect_server/server.hpp"
#include "buf_connect_server/session/session_manager.hpp"
#include "buf_connect_server/auth/stream_token.hpp"
#include "buf_connect_server/auth/middleware.hpp"

// Generated protobuf
#include "buf_connect_server.pb.h"
#include "buf_connect_server/connect/protocol.hpp"

namespace buf_connect_server::services {

// ---------------------------------------------------------------------------
// SessionHandler — construction and route registration
// ---------------------------------------------------------------------------

    SessionHandler::SessionHandler(session::SessionManager& _mgr,
                                   const std::string & _jwt_secret)
            : ServiceHandlerBase(),
            mgr_(_mgr),
            jwt_issuer_(std::make_shared<auth::JwtIssuer>(_jwt_secret)),
            stream_token_(std::make_shared<auth::StreamToken>(_jwt_secret))
            {}

    std::string SessionHandler::ServicePath() const {
        return "/oscilloscope_interface.v2.SessionService";
    }

    void SessionHandler::RegisterRoutes(BufConnectServer& server) {
//        server.RegisterControlRoute(
//                "/buf_connect_server.v2.SessionService/WatchSessionEvents",
//                [this](const connect::ParsedConnectRequest& req,
//                       connect::ConnectResponseWriter& w) {
//                    HandleWatchSessionEvents(req, w);
//                });
        server.RegisterControlRoute(
                "/buf_connect_server.v2.SessionService/GetStreamToken",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) {
                    HandleGetStreamToken(req, w);
                });
//        server.RegisterControlRoute(
//                "/buf_connect_server.v2.SessionService/ClaimActiveRole",
//                [this](const connect::ParsedConnectRequest& req,
//                       connect::ConnectResponseWriter& w) {
//                    HandleClaimActiveRole(req, w);
//                });
//        server.RegisterControlRoute(
//                "/buf_connect_server.v2.SessionService/AdminConflict",
//                [this](const connect::ParsedConnectRequest& req,
//                       connect::ConnectResponseWriter& w) {
//                    HandleAdminConflict(req, w);
//                });
//        server.RegisterControlRoute(
//                "/buf_connect_server.v2.SessionService/Heartbeat",
//                [this](const connect::ParsedConnectRequest& req,
//                       connect::ConnectResponseWriter& w) {
//                    HandleHeartbeat(req, w);
//                });
    }

// ---------------------------------------------------------------------------
// HandleWatchSessionEvents
// ---------------------------------------------------------------------------
// Long-lived server-sent stream.
//
// Changes from the pre-refactor version:
//   - After mgr_.Connect(): check IsSessionInvalidated(session_id).
//     If invalidated, immediately send ForcedLogoutEvent and close the stream.
//   - session_id comes from the JWT ("session_id" claim), NOT from
//     x-connection-id or x-session-id headers.
// ---------------------------------------------------------------------------

//    void SessionHandler::HandleWatchSessionEvents(const connect::Request& req, connect::Response& res)
//    {
//        // ------------------------------------------------------------------
//        // 1. Authenticate via Bearer call_token.
//        // ------------------------------------------------------------------
//        auto claims_opt = auth::ExtractAndVerifyBearer(req, issuer_);
//        if (!claims_opt) {
//            spdlog::warn("session_handler: WatchSessionEvents — missing or invalid call_token");
//            res.SetStatus(401);
//            res.SetError("unauthenticated", "valid call_token required");
//            return;
//        }
//
//        const auth::JwtClaims& claims = *claims_opt;
//
//        if (claims.type != auth::kTokenTypeCallToken) {
//            spdlog::warn("session_handler: WatchSessionEvents — wrong token type '{}'",
//                         claims.type);
//            res.SetStatus(401);
//            res.SetError("unauthenticated", "wrong token type");
//            return;
//        }
//
//        const std::string& user_id    = claims.sub;
//        const std::string& session_id = claims.session_id;  // from JWT, not from header
//        const std::string  role_str   = claims.role;
//
//        // ------------------------------------------------------------------
//        // 2. Derive UserRole from JWT claim.
//        // ------------------------------------------------------------------
//        session::UserRole role = (role_str == "admin")
//                                 ? session::UserRole::Admin
//                                 : session::UserRole::Engineer;
//
//        // ------------------------------------------------------------------
//        // 3. Assign a unique connection_id for this stream instance.
//        //    We use a combination of session_id and a monotonic counter so
//        //    that reconnects from the same session_id can be distinguished.
//        // ------------------------------------------------------------------
//        static std::atomic<uint64_t> conn_counter{0};
//        std::string connection_id = session_id + ":" +
//                                    std::to_string(conn_counter.fetch_add(1));
//
//        spdlog::info("session_handler: WatchSessionEvents opening "
//                     "user='{}' session='{}' conn='{}'",
//                     user_id, session_id, connection_id);
//
//        // ------------------------------------------------------------------
//        // 4. Set up the streaming response.
//        //    The callback captures the Response reference; it must not outlive
//        //    this stack frame (the server framework guarantees this).
//        // ------------------------------------------------------------------
//        res.SetStatus(200);
//        res.StartStream();
//
//        auto send_event = [&res](const buf_connect_server::v2::SessionEvent& ev) -> bool {
//            return res.SendStreamMessage(ev);
//        };
//
//        // ------------------------------------------------------------------
//        // 5. Connect to SessionManager.
//        // ------------------------------------------------------------------
//        session::SessionMode mode = mgr_.CreateSession(session_id, connection_id,
//                                                       user_id, role, send_event);
//
//        // ------------------------------------------------------------------
//        // 6. Post-connect invalidation check.
//        //    Connect() itself checks for invalidation before registering the
//        //    session.  This second check catches the race where TakeOver fires
//        //    between the Connect return and here.
//        //    The SessionStartedEvent has already been sent; sending a
//        //    ForcedLogoutEvent right after is correct — the client must honour
//        //    the last event.
//        // ------------------------------------------------------------------
//        if (mode == session::SessionMode::Observer &&
//            mgr_.IsSessionInvalidated(session_id)) {
//            spdlog::info("session_handler: WatchSessionEvents — session '{}' is "
//                         "invalidated, closing stream immediately", session_id);
//            // ForcedLogoutEvent was already sent by Connect(); close the stream.
//            res.EndStream();
//            mgr_.Disconnect(session_id, user_id);
//            return;
//        }
//
//        // ------------------------------------------------------------------
//        // 7. Block until the client disconnects or the stream is ended by the
//        //    session manager (e.g. ForcedLogoutEvent closes the pipe).
//        // ------------------------------------------------------------------
//        res.WaitForStreamClose();
//
//        // ------------------------------------------------------------------
//        // 8. Disconnect from SessionManager.
//        // ------------------------------------------------------------------
//        spdlog::info("session_handler: WatchSessionEvents closing "
//                     "user='{}' session='{}'", user_id, session_id);
//        mgr_.Disconnect(session_id, user_id);
//        res.EndStream();
//    }

// ---------------------------------------------------------------------------
// HandleGetStreamToken
// ---------------------------------------------------------------------------
// Issues a short-lived HMAC stream token so the client can open a data-plane
// StreamData RPC without sending a long-lived credential.

    void SessionHandler::HandleGetStreamToken(const connect::ParsedConnectRequest & req, connect::ConnectResponseWriter & _writer)
    {
        namespace c = connect;
        namespace a = buf_connect_server::auth;
        // - Extract and validate call token

        auto call_token = c::ExtractAuthorizationBearer(req);
        if (call_token.empty()) {
            _writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            _writer.WriteError(std::string(c::kCodeUnauthenticated), "missing call token");
            return;
        }

        auto call_token_claims = jwt_issuer_->Verify(call_token);
        if (!call_token_claims || call_token_claims->type != "call_token") {
            _writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            _writer.WriteError(std::string(c::kCodeUnauthenticated), "invalid call token");
            return;
        }

        // - Check session is valid and running
        if (!mgr_.HasLiveSession(call_token_claims->user_uuid)) {
            SPDLOG_ERROR("No live session for user {}", call_token_claims->user_uuid);
            _writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            _writer.WriteError(std::string(c::kCodeUnauthenticated), "No live session for that user");
            return;
        }

        // - Issue a stream token
        SPDLOG_INFO("Stream token issued for session {}", call_token_claims->session_uuid);
        a::StreamTokenClaims stream_token_claims;
        stream_token_claims.session_uuid = call_token_claims->session_uuid;
        auto now = std::chrono::system_clock::now();
        stream_token_claims.issued_at = now;
        stream_token_claims.expires_at = now;
        stream_token_claims.decimation_rate = 1.0;
        auto stream_token = stream_token_->Issue(stream_token_claims);

        // - Serialize to protobuf
        buf_connect_server::v2::GetStreamTokenResponse resp;
        resp.set_stream_token(stream_token);

        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));

        _writer.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                                 std::span<const uint8_t>(out));
    }

// ---------------------------------------------------------------------------
// HandleHeartbeat
// ---------------------------------------------------------------------------
// Updates the last-heartbeat timestamp for the session identified in the JWT.
// No changes from pre-refactor behaviour.

//    void SessionHandler::HandleHeartbeat(const connect::Request& req, connect::Response & res)
//    {
//        auto claims_opt = auth::ExtractAndVerifyBearer(req, issuer_);
//        if (!claims_opt) {
//            res.SetStatus(401);
//            res.SetError("unauthenticated", "valid call_token required");
//            return;
//        }
//
//        const auth::JwtClaims& claims = *claims_opt;
//        if (claims.type != auth::kTokenTypeCallToken) {
//            res.SetStatus(401);
//            res.SetError("unauthenticated", "wrong token type");
//            return;
//        }
//
//        // Decode request body — the session_id field in the body is informational;
//        // the authoritative session_id comes from the JWT.
//        buf_connect_server::v2::HeartbeatRequest hb_req;
//        req.DecodeBody(hb_req); // tolerate failure — session_id comes from JWT
//
//        const std::string& session_id = claims.session_id;
//
//        mgr_.UpdateHeartbeat(session_id);
//
//        buf_connect_server::v2::HeartbeatResponse hb_res;
//        res.SetStatus(200);
//        res.EncodeBody(hb_res);
//
//        spdlog::debug("session_handler: Heartbeat session='{}'", session_id);
//    }

// ---------------------------------------------------------------------------
// HandleClaimActiveRole
// ---------------------------------------------------------------------------
// Attempts to promote an Observer session to an active role.
//
// Changed from pre-refactor:
//   - session_id is now read from the JWT ("session_id" claim) rather than
//     from the x-connection-id HTTP header.
// ---------------------------------------------------------------------------

//    void SessionHandler::HandleClaimActiveRole(const connect::Request & req, connect::Response & res)
//    {
//        // ------------------------------------------------------------------
//        // 1. Authenticate.
//        // ------------------------------------------------------------------
//        auto claims_opt = auth::ExtractAndVerifyBearer(req, issuer_);
//        if (!claims_opt) {
//            res.SetStatus(401);
//            res.SetError("unauthenticated", "valid call_token required");
//            return;
//        }
//
//        const auth::JwtClaims& claims = *claims_opt;
//        if (claims.type != auth::kTokenTypeCallToken) {
//            res.SetStatus(401);
//            res.SetError("unauthenticated", "wrong token type");
//            return;
//        }
//
//        // ------------------------------------------------------------------
//        // 2. Read session_id from JWT (NOT from x-connection-id header).
//        // ------------------------------------------------------------------
//        const std::string& session_id = claims.session_id;
//
//        spdlog::info("session_handler: ClaimActiveRole session='{}'", session_id);
//
//        // ------------------------------------------------------------------
//        // 3. Ask SessionManager to promote the session.
//        // ------------------------------------------------------------------
//        session::SessionMode new_mode = mgr_.ClaimActiveRole(session_id);
//
//        // ------------------------------------------------------------------
//        // 4. Respond.
//        // ------------------------------------------------------------------
//        buf_connect_server::v2::ClaimActiveRoleResponse role_res;
//        role_res.set_session_mode(
//                static_cast<buf_connect_server::v2::SessionMode>(new_mode));
//
//        res.SetStatus(200);
//        res.EncodeBody(role_res);
//    }

// ---------------------------------------------------------------------------
// HandleAdminConflict
// ---------------------------------------------------------------------------
// The incumbent admin responds to a conflict challenge.
//
// Changed from pre-refactor:
//   - session_id is now read from the JWT ("session_id" claim) rather than
//     from the x-connection-id HTTP header.
// ---------------------------------------------------------------------------

//    void SessionHandler::HandleAdminConflict(const connect::Request & req, connect::Response & res)
//    {
//        // ------------------------------------------------------------------
//        // 1. Authenticate.
//        // ------------------------------------------------------------------
//        auto claims_opt = auth::ExtractAndVerifyBearer(req, issuer_);
//        if (!claims_opt) {
//            res.SetStatus(401);
//            res.SetError("unauthenticated", "valid call_token required");
//            return;
//        }
//
//        const auth::JwtClaims& claims = *claims_opt;
//        if (claims.type != auth::kTokenTypeCallToken) {
//            res.SetStatus(401);
//            res.SetError("unauthenticated", "wrong token type");
//            return;
//        }
//
//        // ------------------------------------------------------------------
//        // 2. Decode request body.
//        // ------------------------------------------------------------------
//        buf_connect_server::v2::AdminConflictRequest conflict_req;
//        if (!req.DecodeBody(conflict_req)) {
//            res.SetStatus(400);
//            res.SetError("invalid_request", "Failed to decode AdminConflictRequest");
//            return;
//        }
//
//        // ------------------------------------------------------------------
//        // 3. Read session_id from JWT (NOT from x-connection-id header).
//        // ------------------------------------------------------------------
//        const std::string& session_id = claims.session_id;
//
//        spdlog::info("session_handler: AdminConflict session='{}' choice={}",
//                     session_id, static_cast<int>(conflict_req.choice()));
//
//        // ------------------------------------------------------------------
//        // 4. Forward to SessionManager.
//        // ------------------------------------------------------------------
//        mgr_.HandleAdminConflictChoice(session_id,
//                                       static_cast<int>(conflict_req.choice()));
//
//        // ------------------------------------------------------------------
//        // 5. Respond.
//        // ------------------------------------------------------------------
//        buf_connect_server::v2::AdminConflictResponse conflict_res;
//        res.SetStatus(200);
//        res.EncodeBody(conflict_res);
//    }

} // namespace buf_connect_server::services
