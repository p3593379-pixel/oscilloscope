#ifndef BUF_CONNECT_SERVER_AUTH_JWT_HPP
#define BUF_CONNECT_SERVER_AUTH_JWT_HPP

#include <optional>
#include <string>

namespace buf_connect_server::auth {

    struct JwtClaims {
        std::string sub;
        std::string role;
        std::string session_mode;
        std::string session_id;
        std::string type;  // "access" | "refresh"
        int64_t     iat = 0;
        int64_t     exp = 0;
    };

    class JwtUtils {
    public:
        explicit JwtUtils(std::string secret, std::string issuer = "bufconnectserver");

        std::string IssueToken(const JwtClaims& claims) const;

        // Returns nullopt if token is invalid or expired.
        std::optional<JwtClaims> ValidateToken(const std::string& token) const;

    private:
        std::string secret_;
        std::string issuer_;
    };

}  // namespace buf_connect_server::auth

#endif