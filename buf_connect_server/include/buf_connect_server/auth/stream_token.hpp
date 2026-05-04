#ifndef BUF_CONNECT_SERVER_AUTH_STREAM_TOKEN_HPP
#define BUF_CONNECT_SERVER_AUTH_STREAM_TOKEN_HPP

#include <optional>
#include <string>
#include <cstdint>
#include <chrono>

namespace buf_connect_server::auth {

    struct StreamTokenClaims {
        std::string session_uuid;
        std::chrono::system_clock::time_point issued_at;
        std::chrono::system_clock::time_point expires_at;
        double      decimation_rate = 1;
    };

    class StreamToken {
    public:
        // jwt_secret is the master secret; HKDF derives stream-token HMAC key.
        explicit StreamToken(const std::string& jwt_secret);

        [[nodiscard]] std::string Issue(const StreamTokenClaims& claims) const;

        // Returns nullopt if token is invalid, expired, or wrong type.
        [[nodiscard]] std::optional<StreamTokenClaims> Validate(const std::string& token) const;

    private:
        std::string derived_key_;

        static std::string DeriveKey(const std::string& jwt_secret);
    };

}  // namespace buf_connect_server::auth

#endif  // BUF_CONNECT_SERVER_AUTH_STREAM_TOKEN_HPP
