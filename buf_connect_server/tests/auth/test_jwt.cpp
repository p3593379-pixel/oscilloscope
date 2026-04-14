// FILE: buf_connect_server/tests/auth/test_jwt.cpp
#include "buf_connect_server/auth/jwt.hpp"
#include <gtest/gtest.h>
#include <chrono>

using namespace buf_connect_server::auth;

class JwtTest : public ::testing::Test {
protected:
    JwtUtils jwt_{"super_secret_key_for_testing_32b"};
};

TEST_F(JwtTest, IssueAndValidate) {
auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
JwtClaims claims;
claims.sub          = "user-123";
claims.role         = "admin";
claims.session_mode = "active_admin";
claims.type         = "access";
claims.iat          = now;
claims.exp          = now + 900;
auto token = jwt_.IssueToken(claims);
EXPECT_FALSE(token.empty());
auto validated = jwt_.ValidateToken(token);
ASSERT_TRUE(validated.has_value());
EXPECT_EQ(validated->sub,  "user-123");
EXPECT_EQ(validated->role, "admin");
}

TEST_F(JwtTest, ExpiredTokenRejected) {
auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
JwtClaims claims;
claims.sub = "user-123"; claims.role = "admin";
claims.session_mode = "active_admin"; claims.type = "access";
claims.iat = now - 200; claims.exp = now - 100;
auto token     = jwt_.IssueToken(claims);
auto validated = jwt_.ValidateToken(token);
EXPECT_FALSE(validated.has_value());
}

TEST_F(JwtTest, TamperedTokenRejected) {
auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
JwtClaims claims;
claims.sub = "user-123"; claims.role = "engineer";
claims.session_mode = "active"; claims.type = "access";
claims.iat = now; claims.exp = now + 900;
auto token = jwt_.IssueToken(claims);
token.back() ^= 0xFF;  // tamper
auto validated = jwt_.ValidateToken(token);
EXPECT_FALSE(validated.has_value());
}

TEST_F(JwtTest, WrongSecretRejected) {
JwtUtils other_jwt{"different_secret_key_for_testing_!"};
auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
JwtClaims claims;
claims.sub = "u"; claims.role = "admin";
claims.session_mode = ""; claims.type = "access";
claims.iat = now; claims.exp = now + 900;
auto token     = other_jwt.IssueToken(claims);
auto validated = jwt_.ValidateToken(token);
EXPECT_FALSE(validated.has_value());
}
