#include "buf_connect_server/auth/middleware.hpp"
#include "buf_connect_server/connect/request.hpp"
#include <chrono>

buf_connect_server::auth::AuthMiddleware::AuthMiddleware(
        std::shared_ptr<JwtUtils> jwt_utils)
        : jwt_utils_(std::move(jwt_utils)) {}

std::optional<buf_connect_server::auth::ConnectContext>
buf_connect_server::auth::AuthMiddleware::Authenticate(
        const std::string& authorization_header) const {
    auto token = connect::ExtractBearerToken(authorization_header);
    if (token.empty()) return std::nullopt;
    auto claims = jwt_utils_->ValidateToken(token);
    if (!claims) return std::nullopt;
    if (claims->type != "access") return std::nullopt;

    ConnectContext ctx;
    ctx.user_id       = claims->sub;
    ctx.role          = claims->role;
    ctx.session_mode  = claims->session_mode;
    ctx.session_id = claims->session_id;
    ctx.exp        = claims->exp;
    return ctx;
}
