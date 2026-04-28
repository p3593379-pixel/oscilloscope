#include "buf_connect_server/auth/middleware.hpp"
#include "buf_connect_server/connect/request.hpp"
#include <chrono>

buf_connect_server::auth::AuthMiddleware::AuthMiddleware(
        std::shared_ptr<JwtIssuer> _jwt_issuer)
        : jwt_issuer_(std::move(_jwt_issuer)) {}

std::optional<buf_connect_server::auth::ConnectContext>
buf_connect_server::auth::AuthMiddleware::Authenticate(const std::string& authorization_header) const
{
    auto token = connect::ExtractBearerToken(authorization_header);
    if (token.empty()) return std::nullopt;
    auto claims = jwt_issuer_->Verify(token);
    if (!claims) return std::nullopt;
    if (claims->type != "call_token") return std::nullopt;  // FIXED: was "access"

    ConnectContext ctx;
    ctx.user_id      = claims->sub;
    ctx.role         = claims->role;
    ctx.session_id   = claims->session_id;
    ctx.exp          = std::chrono::duration_cast<std::chrono::seconds>(claims->expires_at - claims->issued_at).count();
    return ctx;
}
