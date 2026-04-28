#include "buf_connect_server/services/auth_handler.hpp"
#include "buf_connect_server.pb.h"
#include "buf_connect_server/connect/frame_codec.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include <chrono>
#include <sstream>

#include <spdlog/spdlog.h>

namespace buf_connect_server {
    namespace services {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

        AuthHandler::AuthHandler(auth::UserStore&         user_store,
                                 const AuthConfig&        auth_config,
                                 session::SessionManager& mgr)
                : user_store_(user_store),
                  auth_config_(auth_config),
                  mgr_(mgr),
                  jwt_issuer_(std::make_shared<auth::JwtIssuer>(auth_config.jwt_secret)) {}

        void AuthHandler::RegisterRoutes(BufConnectServer& server) {
            server.RegisterControlRoute(
                    "/buf_connect_server.v2.AuthService/Login",
                    [this](const connect::ParsedConnectRequest& req,
                           connect::ConnectResponseWriter& w) { HandleLogin(req, w); });
            server.RegisterControlRoute(
                    "/buf_connect_server.v2.AuthService/RenewCallToken",
                    [this](const connect::ParsedConnectRequest& req,
                           connect::ConnectResponseWriter& w) { HandleRenewCallToken(req, w); });
            server.RegisterControlRoute(
                    "/buf_connect_server.v2.AuthService/TakeOver",
                    [this](const connect::ParsedConnectRequest& req,
                           connect::ConnectResponseWriter& w) { HandleTakeOver(req, w); });
        }


// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

        std::string AuthHandler::IssueCallToken(const std::string& user_id,
                                                const std::string& role,
                                                const std::string& session_id) const
        {
            auto now = std::chrono::system_clock::now();
            buf_connect_server::auth::JwtClaims claims;
            claims.sub          = user_id;
            claims.role         = role;
            claims.session_id   = session_id;
            claims.type         = "call_token";
            claims.issued_at          = now;
            claims.expires_at          = now + std::chrono::seconds(auth_config_.call_token_ttl_seconds);
            return jwt_issuer_->Issue(claims);
        }

        std::string AuthHandler::IssueSessionTicket(const std::string & user_id,
                                                    const std::string& role,
                                                    const std::string& session_id) const
        {
            auto now = std::chrono::system_clock::now();

            buf_connect_server::auth::JwtClaims claims;
            claims.sub          = user_id;
            claims.role         = role;
            claims.session_id   = session_id;
            claims.type         = "session_ticket";
            claims.issued_at          = now;
            claims.expires_at          = now + std::chrono::seconds(auth_config_.session_ticket_ttl_seconds);
            return jwt_issuer_->Issue(claims);
        }

// static
        std::string AuthHandler::ExtractCookie(const connect::ParsedConnectRequest& req, const std::string& name)
        {
            auto cookie_it = req.headers.find("cookie");
            if (cookie_it == req.headers.end()) {
                SPDLOG_ERROR("No \'cookie\' header in request");
                return {};
            }
            const std::string & cookies = cookie_it->second;
            const std::string prefix = name + "=";
            std::istringstream ss(cookies);
            std::string token;
            while (std::getline(ss, token, ';')) {
                // Trim leading whitespace
                const auto start = token.find_first_not_of(' ');
                if (start == std::string::npos)
                    continue;
                token = token.substr(start);
                if (token.rfind(prefix, 0) == 0) {
                    return token.substr(prefix.size());
                }
            }
            return {};
        }
        std::string AuthHandler::BuildSessionTicketCookie(const std::string & token)
        {
            return {"call_token=" + token};
        }

        static std::vector<uint8_t> ExtractUnaryBody(const connect::ParsedConnectRequest& req)
        {
            if (req.body.size() < 5)
                return req.body;
            auto decoded = buf_connect_server::connect::DecodeFrame(std::span<const uint8_t>(req.body));
            if (decoded.bytes_consumed > 0)
                return {decoded.payload.begin(), decoded.payload.end()};
            return req.body;
        }


// ---------------------------------------------------------------------------
// HandleLogin
// ---------------------------------------------------------------------------

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
            // 1. Authenticate credentials
            auto user = user_store_.FindByUsername(login_req.username());
            if (!user || !user_store_.VerifyPassword(user->id, login_req.password())) {
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "invalid credentials");
                return;
            }
            user_store_.UpdateLastLogin(user->id);

            const auto user_id = std::stoull(user->id);
            const auto role= user->role;

//            TODO: WHAT IS THAT CALLBACK!!
            // 2. Create a new session (may surface a conflict)
            auto new_session_entry = mgr_.CreateSession(user_id, role, [](const v2::SessionEvent & _event) { return true; });

            // TODO: CONFLICT LOGIC
            /*if (result.conflict) {
                // Conflict path: issue a session_ticket so the client can call TakeOver.
                // The cookie carries the new (pending) session_id.
                const std::string new_session_id = result.new_session_id;
                const std::string session_mode   = result.session_mode;

                const std::string ticket = IssueSessionTicket(user_id, role, session_mode, new_session_id);

                {
                    std::lock_guard<std::mutex> lock(pending_mutex_);
                    pending_logins_[new_session_id] = PendingLogin{user_id, new_session_id, role};
                }

                // TODO: adapt response writing to match the existing handler pattern
                resp.SetStatus(200);
                resp.SetHeader("Set-Cookie", BuildSessionTicketCookie(ticket));
                resp.WriteBody("{\"session_conflict\":true"
                               ",\"conflict_info\":{"
                               "\"started_at_utc\":\"" + result.conflict_started_at + "\""
                                                                                      ",\"role\":\"" + result.conflict_role + "\""
                                                                                                                              "}}");
                return;
            }*/

            // 3. Normal path: issue call_token + session_ticket
            const std::string call_token     = IssueCallToken(user->id, role, new_session_entry.session_uuid);
            const std::string session_ticket = IssueSessionTicket(user->id, role, new_session_entry.session_uuid);

            // TODO: adapt response writing to match the existing handler pattern
            v2::LoginResponse resp;
            resp.set_call_token(call_token);
            resp.set_role(session::to_v2_user_role(new_session_entry.role));
            resp.set_session_mode(session::to_v2_session_mode(new_session_entry.mode));
            resp.set_session_id(new_session_entry.session_uuid);
            resp.set_session_conflict(false);
            resp.set_allocated_conflict_info(nullptr);
            std::vector<uint8_t> out(resp.ByteSizeLong());
            resp.SerializeToArray(out.data(), static_cast<int>(out.size()));

            writer.AddHeader("set-cookie", BuildSessionTicketCookie(session_ticket));
            writer.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,std::span<const uint8_t>(out));
        }

// ---------------------------------------------------------------------------
// HandleRenewCallToken
// ---------------------------------------------------------------------------

        void AuthHandler::HandleRenewCallToken(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& writer) {
            namespace c = connect;
            const std::string raw_ticket = ExtractCookie(req, "session_ticket");
            if (raw_ticket.empty()) {
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "missing session ticket cookie");
                return;
            }

            auto claims = jwt_issuer_->Verify(raw_ticket);
            if (!claims || claims->type != "session_ticket") {
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "invalid session ticket");
                return;
            }

            const std::string user_id      = claims->sub;
            const std::string role         = claims->role;
            const std::string session_id   = claims->session_id;

            if (mgr_.IsSessionInvalidated(session_id)) {
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "session invalidated");
                return;
            }

            // Issue new call_token, rotate session_ticket
            std::string call_token     = IssueCallToken(user_id, role, session_id);
            std::string session_ticket = IssueSessionTicket(user_id, role, session_id);

            mgr_.Touch(session_id);

            v2::RenewCallTokenResponse resp;
            resp.set_call_token(call_token);
            std::vector<uint8_t> out(resp.ByteSizeLong());
            resp.SerializeToArray(out.data(), static_cast<int>(out.size()));

            writer.AddHeader("set-cookie", BuildSessionTicketCookie(session_ticket));
            writer.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                                     std::span<const uint8_t>(out));
        }

// ---------------------------------------------------------------------------
// HandleTakeOver
// ---------------------------------------------------------------------------

        void AuthHandler::HandleTakeOver(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& writer)
        {
            namespace c = connect;
            const std::string raw_ticket = ExtractCookie(req, "session_ticket");
            if (raw_ticket.empty()) {
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "missing session ticket cookie");
                return;
            }

            auto claims = jwt_issuer_->Verify(raw_ticket);
            if (!claims || claims->type != "session_ticket") {
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "invalid session ticket");
                return;
            }

            const std::string user_id      = claims->sub;
            const std::string role         = claims->role;
            const std::string new_session_id = claims->session_id;

            v2::SessionMode session_mode;

            // Verify this session is in the pending-conflict map
            {
                std::lock_guard<std::mutex> lock(pending_mutex_);
                const auto it = pending_logins_.find(new_session_id);
                if (it == pending_logins_.end()) {
                    writer.SendHeaders(c::kHttpConflict, "application/json");
                    writer.WriteError(std::string(c::kCodeUnauthenticated), "no pending takeover for this session");
                    return;
                }
                pending_logins_.erase(it);
            }
            // Inheriting session mode from the taken over session
            session_mode = mgr_.TakeOver(std::stoull(user_id), new_session_id);
            // Evict the incumbent session and activate the new one
            if (session_mode == v2::SessionMode::SESSION_MODE_UNSPECIFIED) {
                writer.SendHeaders(c::kHttpInternalError, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "takeover failed");
                return;
            }

            // Issue fresh call_token + session_ticket for the new session
            std::string call_token     = IssueCallToken(user_id, role, new_session_id);
            std::string session_ticket = IssueSessionTicket(user_id, role, new_session_id);

            v2::TakeOverResponse resp;
            v2::UserRole v2_role = (role == "admin") ? v2::USER_ROLE_ADMIN : v2::USER_ROLE_ENGINEER;
            resp.set_role(v2_role);
            resp.set_call_token(call_token);
            resp.set_session_id(new_session_id);
            resp.set_session_mode(session_mode);
            std::vector<uint8_t> out(resp.ByteSizeLong());
            resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
            writer.AddHeader("set-cookie", BuildSessionTicketCookie(session_ticket));
            writer.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,std::span<const uint8_t>(out));
        }

    }  // namespace services
}  // namespace buf_connect_server
