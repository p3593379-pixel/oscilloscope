// FILE: buf_connect_server/src/auth/stream_token.cpp
#include "buf_connect_server/auth/stream_token.hpp"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <jwt-cpp/jwt.h>
#include <stdexcept>
#include <vector>
#include <chrono>

static const char kHkdfLabel[] = "stream_token";

std::string buf_connect_server::auth::StreamToken::DeriveKey(
        const std::string& jwt_secret) {
    std::vector<uint8_t> okm(32);
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!ctx) throw std::runtime_error("HKDF ctx alloc failed");
    EVP_PKEY_derive_init(ctx);
    EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256());
    EVP_PKEY_CTX_set1_hkdf_key(
            ctx,
            reinterpret_cast<const unsigned char*>(jwt_secret.data()),
            jwt_secret.size());
    // no salt
    EVP_PKEY_CTX_add1_hkdf_info(
            ctx,
            reinterpret_cast<const unsigned char*>(kHkdfLabel),
            sizeof(kHkdfLabel) - 1);
    size_t out_len = okm.size();
    EVP_PKEY_derive(ctx, okm.data(), &out_len);
    EVP_PKEY_CTX_free(ctx);
    return std::string(reinterpret_cast<const char*>(okm.data()), out_len);
}

buf_connect_server::auth::StreamToken::StreamToken(const std::string& jwt_secret)
        : derived_key_(DeriveKey(jwt_secret)) {}

std::string buf_connect_server::auth::StreamToken::Issue(
        const StreamTokenClaims& claims) const {
    return jwt::create()
            .set_subject(claims.sub)
            .set_issued_at(std::chrono::system_clock::from_time_t(claims.iat))
            .set_expires_at(std::chrono::system_clock::from_time_t(claims.exp))
            .set_payload_claim("tier", jwt::claim(claims.tier))
            .set_payload_claim("type", jwt::claim(std::string("stream")))
            .sign(jwt::algorithm::hs256{derived_key_});
}

std::optional<buf_connect_server::auth::StreamTokenClaims>
buf_connect_server::auth::StreamToken::Validate(const std::string& token) const {
    try {
        auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{derived_key_});
        auto decoded = jwt::decode(token);
        verifier.verify(decoded);
        if (decoded.get_payload_claim("type").as_string() != "stream")
            return std::nullopt;
        StreamTokenClaims c;
        c.sub  = decoded.get_subject();
        c.tier = decoded.get_payload_claim("tier").as_string();
        c.iat  = std::chrono::system_clock::to_time_t(decoded.get_issued_at());
        c.exp  = std::chrono::system_clock::to_time_t(decoded.get_expires_at());
        return c;
    } catch (...) {
        return std::nullopt;
    }
}
