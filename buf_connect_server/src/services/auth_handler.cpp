#include "buf_connect_server/services/auth_handler.hpp"
#include "buf_connect_server.pb.h"
#include "buf_connect_server/connect/frame_codec.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include <chrono>
#include <sstream>

#include <spdlog/spdlog.h>
#include <iomanip>

namespace buf_connect_server {
    namespace services {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

        AuthHandler::AuthHandler(auth::UserStore&         user_store,
                                 const std::string & _jwt_secret,
                                 const SessionConfig & _session_config,
                                 session::SessionManager& mgr)
                : user_store_(user_store),
                  mgr_(mgr),
                  kSessionConfig(_session_config),
                  jwt_issuer_(std::make_shared<auth::JwtIssuer>(_jwt_secret)) {}

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

        std::string AuthHandler::IssueCallToken(const session::UserUuid &_user_uuid,
                                                const std::string& _role,
                                                const session::SessionUuid &_session_uuid) const
        {
            auto now = std::chrono::system_clock::now();
            buf_connect_server::auth::JwtClaims claims;
            claims.user_uuid          = _user_uuid;
            claims.role         = _role;
            claims.session_uuid   = _session_uuid;
            claims.type         = "call_token";
            claims.issued_at          = now;
            claims.expires_at         = now + std::chrono::seconds(kSessionConfig.call_token_renew_period);
            return jwt_issuer_->Issue(claims);
        }

// static
//        std::string AuthHandler::ExtractAuthorizationBearer(const connect::ParsedConnectRequest& req)
//        {
//            namespace c = connect;
//            auto auth_it = req.headers.find("authorization");
//            if (auth_it == req.headers.end() || auth_it->second.rfind("Bearer ", 0) != 0) {
//                return {};
//            }
//            return auth_it->second.substr(7); // strip "Bearer "
//        }

//        static std::vector<uint8_t> ExtractUnaryBody(const connect::ParsedConnectRequest& req)
//        {
//            if (req.body.size() < 5)
//                return req.body;
//            auto decoded = buf_connect_server::connect::DecodeFrame(std::span<const uint8_t>(req.body));
//            if (decoded.bytes_consumed > 0)
//                return {decoded.payload.begin(), decoded.payload.end()};
//            return req.body;
//        }


// ---------------------------------------------------------------------------
// HandleLogin
// ---------------------------------------------------------------------------

        void AuthHandler::HandleLogin(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& writer)
        {
            namespace c = connect;

            auto body = c::ExtractUnaryBody(req);
            v2::LoginRequest login_req;
            if (!login_req.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
                writer.SendHeaders(c::kHttpBadRequest, "application/json");
                writer.WriteError(std::string(c::kCodeInvalidArgument),
                                  "failed to parse LoginRequest");
                return;
            }
            // 1. Authenticate credentials
            auto user = user_store_.FindByUsername(login_req.username());
            if (!user || !user_store_.VerifyPassword(user->uuid, login_req.password())) {
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "invalid credentials");
                return;
            }

            // TODO: CONFLICT LOGIC
            if (mgr_.HasLiveSession(user->uuid)) {
                const auto live = mgr_.GetLiveSessionInfo(user->uuid);

                // CreateSession() detects user_session_index_ is occupied and stores the
                // new entry only in sessions_ (pending / Observer), not in user_session_index_.
                // TakeOver() will activate it when the user confirms the takeover.
                auto pending = session::SessionManager::BuildNewSession(
                        user->uuid, user->role,
                        [](const v2::SessionEvent &)
                        { return true; }); // placeholder — SessionHandler wires real cb

                {
                    std::lock_guard<std::mutex> lock(pending_mutex_);
                    pending_logins_[pending.session_uuid] = PendingLogin{user->uuid, pending.session_uuid, user->role};
                }

                // Ticket carries the *pending* session_uuid so TakeOver can look it up.
                const std::string pending_call_token = IssueCallToken(user->uuid, user->role, pending.session_uuid);

                // Convert steady_clock → ISO-8601 UTC string
                const auto diff         = session::Clock::now() - live.started_at;
                const auto started_sys  = std::chrono::time_point_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now() - diff);
                const std::time_t tt    = std::chrono::system_clock::to_time_t(started_sys);
                std::tm tm_utc{};
                gmtime_r(&tt, &tm_utc);
                std::ostringstream oss;
                oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");

                auto* ci = new v2::SessionConflictInfo();
                ci->set_started_at_utc(oss.str());
                ci->set_role(live.role);

                v2::LoginResponse resp;
                resp.set_call_token(pending_call_token);
                resp.set_session_conflict(true);
                resp.set_allocated_conflict_info(ci);  // call_token absent — client must TakeOver first
                std::vector<uint8_t> out(resp.ByteSizeLong());
                resp.SerializeToArray(out.data(), static_cast<int>(out.size()));

                writer.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto, std::span<const uint8_t>(out));
                return;
            }

            user_store_.UpdateLastLogin(user->uuid);

            // 2. Normal path: Create a new session
            auto new_session_entry = session::SessionManager::BuildNewSession(user->uuid, user->role,
                                                                              [](const v2::SessionEvent &_event)
                                                                              { return true; });
            mgr_.RegisterSession(new_session_entry);

            // 3. Normal path: issue call_token + session_ticket
            const std::string call_token     = IssueCallToken(user->uuid, user->role, new_session_entry.session_uuid);

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

            writer.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,std::span<const uint8_t>(out));

            std::string mode_str;
            switch (new_session_entry.mode) {
                case session::SessionMode::ActiveAdmin: { mode_str = "active_admin"; break; }
                case session::SessionMode::Active: { mode_str = "active_engineer"; break; }
                case session::SessionMode::Observer: { mode_str = "observer"; break; }
                case session::SessionMode::OnService: { mode_str = "on_service"; break; }
                default: mode_str = "unspecified";
            }

            SPDLOG_INFO("session_manager: CreateSession session_uuid='{}' user='{}' role={} mode={}",
                         new_session_entry.session_uuid, new_session_entry.user_id, user->role, mode_str);
        }

// ---------------------------------------------------------------------------
// HandleRenewCallToken
// ---------------------------------------------------------------------------

        void AuthHandler::HandleRenewCallToken(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& writer) {
            namespace c = connect;
            const std::string raw_ticket = c::ExtractAuthorizationBearer(req);
            if (raw_ticket.empty()) {
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "missing call token");
                return;
            }

            auto claims = jwt_issuer_->Verify(raw_ticket);
            if (!claims || claims->type != "call_token") {
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "invalid call token");
                return;
            }

            const std::string user_id      = claims->user_uuid;
            const std::string role         = claims->role;
            const std::string session_uuid   = claims->session_uuid;
            SPDLOG_INFO("Renewing token for session {}", session_uuid);

            if (mgr_.IsSessionInvalidated(session_uuid)) {
                SPDLOG_INFO("Unable to renew: Session {} is invalidated", session_uuid);
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "session invalidated");
                return;
            }

            // Issue new call_token, rotate session_ticket
            std::string call_token     = IssueCallToken(user_id, role, session_uuid);

            mgr_.Touch(session_uuid);

            v2::RenewCallTokenResponse resp;
            resp.set_call_token(call_token);
            std::vector<uint8_t> out(resp.ByteSizeLong());
            resp.SerializeToArray(out.data(), static_cast<int>(out.size()));

            writer.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                                     std::span<const uint8_t>(out));
        }

// ---------------------------------------------------------------------------
// HandleTakeOver
// ---------------------------------------------------------------------------

        void AuthHandler::HandleTakeOver(const connect::ParsedConnectRequest& req, connect::ConnectResponseWriter& writer)
        {
            namespace c = connect;
            // 1. Parse request body
            auto body = c::ExtractUnaryBody(req);
            v2::TakeOverRequest takeover_req;
            if (!takeover_req.ParseFromArray(body.data(), static_cast<int>(body.size()))
                || takeover_req.call_token().empty()) {
                writer.SendHeaders(c::kHttpBadRequest, "application/json");
                writer.WriteError(std::string(c::kCodeInvalidArgument), "missing call_token");
                return;
            }

            // 2. Verify JWT — signature proves server issued it, claims give us the uuid
            auto claims = jwt_issuer_->Verify(takeover_req.call_token());
            if (!claims || claims->type != "call_token") {
                writer.SendHeaders(c::kHttpUnauthorized, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "invalid call token");
                return;
            }
            const std::string user_uuid        = claims->user_uuid;
            const std::string user_role        = claims->role;
            const std::string new_session_uuid = claims->session_uuid;
            SPDLOG_INFO("Session {} is attempting a takeover", new_session_uuid);

            v2::SessionMode session_mode;

            // Verify this session is in the pending-conflict map
            {
                std::lock_guard<std::mutex> lock(pending_mutex_);
                const auto it = pending_logins_.find(new_session_uuid);
                if (it == pending_logins_.end()) {
                    writer.SendHeaders(c::kHttpConflict, "application/json");
                    writer.WriteError(std::string(c::kCodeUnauthenticated), "no pending takeover for this session");
                    return;
                }
                pending_logins_.erase(it);
            }
            // Inheriting session mode from the taken over session
            session_mode = mgr_.TakeOver(user_uuid, new_session_uuid);
            if (session_mode == v2::SessionMode::SESSION_MODE_UNSPECIFIED) {
                writer.SendHeaders(c::kHttpInternalError, "application/json");
                writer.WriteError(std::string(c::kCodeUnauthenticated), "takeover failed");
                return;
            }

            // Issue fresh call_token + session_ticket for the new session
            std::string call_token     = IssueCallToken(user_uuid, user_role, new_session_uuid);

            v2::TakeOverResponse resp;
            v2::UserRole v2_role = (user_role == "admin") ? v2::USER_ROLE_ADMIN : v2::USER_ROLE_ENGINEER;
            resp.set_role(v2_role);
            resp.set_call_token(call_token);
            resp.set_session_id(new_session_uuid);
            resp.set_session_mode(session_mode);
            std::vector<uint8_t> out(resp.ByteSizeLong());
            resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
            writer.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,std::span<const uint8_t>(out));
        }

    }  // namespace services
}  // namespace buf_connect_server
