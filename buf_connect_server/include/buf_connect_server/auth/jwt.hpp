#ifndef BUF_CONNECT_SERVER_AUTH_JWT_HPP
#define BUF_CONNECT_SERVER_AUTH_JWT_HPP

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace buf_connect_server::auth {

// ---------------------------------------------------------------------------
// JWT type claim constants
// ---------------------------------------------------------------------------
// Every JWT issued by this library carries a custom "typ" claim (not the JOSE
// header "typ") that identifies which token class it belongs to.  Handlers
// MUST verify this claim to prevent token-class confusion attacks — e.g. a
// client must not be able to present a session_ticket where a call_token is
// expected.
//
//   call_token     — short-lived bearer credential.  Sent as
//                    "Authorization: Bearer <token>" on every RPC.
//                    Previously named "access_token".
//
//   session_ticket — long-lived credential stored in an HttpOnly cookie.
//                    Used exclusively to obtain a fresh call_token via the
//                    RenewCallToken RPC.  May also be presented to the
//                    TakeOver RPC to authenticate a forced session takeover.
//                    Previously named "refresh_token".
//                    Cookie path: /buf_connect_server.v2.AuthService/RenewCallToken

/// Type claim value for a call_token JWT.
    inline constexpr std::string_view kTokenTypeCallToken    = "call_token";

/// Type claim value for a session_ticket JWT.
    inline constexpr std::string_view kTokenTypeSessionTicket = "session_ticket";

// ---------------------------------------------------------------------------
// JwtClaims
// ---------------------------------------------------------------------------
// Parsed representation of a verified JWT.  All fields correspond to
// registered or private claims in the token payload.

    struct JwtClaims {
        /// "sub" — user ID (UUID string).
        std::string sub;

        /// "username" — human-readable username.
        std::string username;

        /// "role" — "engineer" or "admin".
        std::string role;

        /// "session_id" — UUID of the session this token belongs to.
        std::string session_id;

        /// "typ" — token class; must be kTokenTypeCallToken or kTokenTypeSessionTicket.
        std::string type;

        /// "exp" — absolute expiry as a Unix timestamp.
        std::chrono::system_clock::time_point expires_at;

        /// "iat" — issuance time.
        std::chrono::system_clock::time_point issued_at;
    };

// ---------------------------------------------------------------------------
// JwtIssuer
// ---------------------------------------------------------------------------
// Signs and verifies JWTs using HS256.
//
// Usage:
//   JwtIssuer issuer(secret);
//   std::string tok = issuer.Issue(claims, ttl_seconds);
//   auto parsed     = issuer.Verify(tok);

    class JwtIssuer {
    public:
        explicit JwtIssuer(std::string secret);

        /// Issue a JWT with the provided claims, expiring `ttl_seconds` from now.
        /// The "typ" private claim is set from `claims.type`.
        [[nodiscard]] std::string Issue(const JwtClaims &claims) const;

        /// Verify and parse a JWT.  Returns std::nullopt on failure (bad signature,
        /// expired, or malformed).
        [[nodiscard]] std::optional<JwtClaims> Verify(const std::string& token) const;
    private:
        std::string secret_;
        const std::string kIssuer{"bufconnectserver"};
    };

// ---------------------------------------------------------------------------
// CookieBuilder
// ---------------------------------------------------------------------------
// Builds the Set-Cookie header value for a session_ticket cookie.
//
//   BuildSessionTicketCookie(token, max_age_seconds)
//
// Produces:
//   session_ticket=<token>; Path=/buf_connect_server.v2.AuthService/RenewCallToken;
//   HttpOnly; SameSite=Strict; Secure; Max-Age=<max_age_seconds>
//
// Previously, the cookie was named "refresh_token" and scoped to
// /buf_connect_server.v2.AuthService/Refresh.

/// Builds a fully-formed Set-Cookie header value for a session_ticket.
    [[nodiscard]] std::string BuildSessionTicketCookie(const std::string& token);

// ---------------------------------------------------------------------------
// CookieParser
// ---------------------------------------------------------------------------
// Extracts the session_ticket value from a raw Cookie header string.
// Returns std::nullopt if the cookie is absent.

    [[nodiscard]] std::optional<std::string> ExtractSessionTicketCookie(
            const std::string& cookie_header);

}  // namespace buf_connect_server::auth

#endif