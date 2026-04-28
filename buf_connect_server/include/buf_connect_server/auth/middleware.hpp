// FILE: buf_connect_server/include/buf_connect_server/auth/middleware.hpp
#ifndef BUF_CONNECT_SERVER_AUTH_MIDDLEWARE_HPP
#define BUF_CONNECT_SERVER_AUTH_MIDDLEWARE_HPP

#include "buf_connect_server/auth/jwt.hpp"
#include <memory>
#include <optional>
#include <string>

namespace buf_connect_server::auth {

struct ConnectContext {
  std::string user_id;
  std::string role;
  std::string session_id;      // token's session_id alias
  int64_t     exp = 0;         // token expiry, used by heartbeat timeout logic
};

class AuthMiddleware {
 public:
  explicit AuthMiddleware(std::shared_ptr<JwtIssuer> _jwt_issuer);

  // Extract and validate the Bearer JWT from the Authorization header.
  // Returns nullopt if missing or invalid.
  std::optional<ConnectContext> Authenticate(const std::string& authorization_header) const;

 private:
  std::shared_ptr<JwtIssuer> jwt_issuer_;
};

}  // namespace buf_connect_server::auth

#endif  // BUF_CONNECT_SERVER_AUTH_MIDDLEWARE_HPP
