#ifndef BUF_CONNECT_SERVER_AUTH_STREAM_TOKEN_HPP
#define BUF_CONNECT_SERVER_AUTH_STREAM_TOKEN_HPP

#include <optional>
#include <string>
#include <cstdint>

namespace buf_connect_server::auth {

    struct StreamTokenClaims {
        std::string sub;
        std::string tier;  // "full" | "preview"
        int64_t     iat = 0;
        int64_t     exp = 0;
    };

    class StreamToken {
    public:
        // jwt_secret is the master secret; HKDF derives stream-token HMAC key.
        explicit StreamToken(const std::string& jwt_secret);

        std::string Issue(const StreamTokenClaims& claims) const;

        // Returns nullopt if token is invalid, expired, or wrong type.
        std::optional<StreamTokenClaims> Validate(const std::string& token) const;

    private:
        std::string derived_key_;

        static std::string DeriveKey(const std::string& jwt_secret);
    };

}  // namespace buf_connect_server::auth

#endif  // BUF_CONNECT_SERVER_AUTH_STREAM_TOKEN_HPP
