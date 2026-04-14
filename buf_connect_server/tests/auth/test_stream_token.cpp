// FILE: buf_connect_server/tests/auth/test_stream_token.cpp
#include "buf_connect_server/auth/stream_token.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <jwt-cpp/jwt.h>

using namespace buf_connect_server::auth;

class StreamTokenTest : public ::testing::Test {
protected:
    StreamToken st_{"super_secret_key_for_testing_32b"};
};

TEST_F(StreamTokenTest, IssueAndValidate) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    StreamTokenClaims c;
    c.sub  = "user-abc";
    c.tier = "full";
    c.iat  = now;
    c.exp  = now + 30;
    auto token    = st_.Issue(c);
    auto validated = st_.Validate(token);
    ASSERT_TRUE(validated.has_value());
    EXPECT_EQ(validated->sub,  "user-abc");
    EXPECT_EQ(validated->tier, "full");
}

TEST_F(StreamTokenTest, PreviewTierRoundTrip) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    StreamTokenClaims c;
    c.sub  = "user-xyz";
    c.tier = "preview";
    c.iat  = now;
    c.exp  = now + 30;
    auto token    = st_.Issue(c);
    auto validated = st_.Validate(token);
    ASSERT_TRUE(validated.has_value());
    EXPECT_EQ(validated->tier, "preview");
}

TEST_F(StreamTokenTest, ExpiredTokenRejected) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    StreamTokenClaims c;
    c.sub = "user-abc"; c.tier = "full";
    c.iat = now - 60;   c.exp  = now - 1;
    auto token    = st_.Issue(c);
    auto validated = st_.Validate(token);
    EXPECT_FALSE(validated.has_value());
}

TEST_F(StreamTokenTest, TamperedTokenRejected) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    StreamTokenClaims c;
    c.sub = "user-abc"; c.tier = "full";
    c.iat = now;        c.exp  = now + 30;
    auto token = st_.Issue(c);
    token.back() ^= 0xFF;
    auto validated = st_.Validate(token);
    EXPECT_FALSE(validated.has_value());
}

TEST_F(StreamTokenTest, DifferentSecretProducesIncompatibleToken) {
    StreamToken st2{"different_secret_key_for_testing!"};
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    StreamTokenClaims c;
    c.sub = "u"; c.tier = "full"; c.iat = now; c.exp = now + 30;
    auto token    = st2.Issue(c);
    auto validated = st_.Validate(token);
    EXPECT_FALSE(validated.has_value());
}

TEST_F(StreamTokenTest, DerivedKeyDiffersFromJwtKey) {
    // Issue a JWT-signed token with the raw secret, try to validate as stream token
    // — must fail because stream token uses HKDF-derived key
    using namespace jwt;
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto raw_token = jwt::create()
            .set_subject("user")
            .set_issued_at(std::chrono::system_clock::from_time_t(now))
            .set_expires_at(std::chrono::system_clock::from_time_t(now + 30))
            .set_payload_claim("tier", jwt::claim(std::string("full")))
            .set_payload_claim("type", jwt::claim(std::string("stream")))
            .sign(jwt::algorithm::hs256{"super_secret_key_for_testing_32b"});
    auto validated = st_.Validate(raw_token);
    EXPECT_FALSE(validated.has_value());
}
