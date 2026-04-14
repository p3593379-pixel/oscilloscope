// FILE: buf_connect_server/tests/integration/test_unary_rpc.cpp
#include "buf_connect_server/connect/response_writer.hpp"
#include "buf_connect_server/connect/request.hpp"
#include "buf_connect_server/connect/frame_codec.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include "buf_connect_server/auth/jwt.hpp"
#include "buf_connect_server/auth/user_store.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <vector>
#include <string>

using namespace buf_connect_server;

// ── Harness ──────────────────────────────────────────────────────────────────

struct RpcHarness {
  std::vector<std::pair<std::string, std::string>> sent_headers;
  std::vector<std::vector<uint8_t>>                sent_body;
  bool disconnected = false;

  connect::ConnectResponseWriter MakeWriter(bool streaming = false) {
    connect::WriteCallback wfn = [this](std::span<const uint8_t> d) -> bool {
      if (disconnected) return false;
      sent_body.emplace_back(d.begin(), d.end());
      return true;
    };
    connect::HeaderCallback hfn = [this](const std::string& k, const std::string& v) {
      sent_headers.emplace_back(k, v);
    };
    return connect::ConnectResponseWriter(wfn, hfn, streaming);
  }

  std::string StatusHeader() const {
    for (const auto& [k, v] : sent_headers)
      if (k == ":status") return v;
    return "";
  }

  std::string ContentTypeHeader() const {
    for (const auto& [k, v] : sent_headers)
      if (k == "content-type") return v;
    return "";
  }
};

// ── JWT round-trip through writer ────────────────────────────────────────────

TEST(UnaryRpcTest, JwtTokenFitsInUnaryResponse) {
  auth::JwtUtils jwt("secret_key_32bytes_for_testing!!");
  auth::JwtClaims claims;
  auto now = std::time(nullptr);
  claims.sub          = "user-001";
  claims.role         = "engineer";
  claims.session_mode = "active";
  claims.type         = "access";
  claims.iat          = now;
  claims.exp          = now + 900;
  auto token = jwt.IssueToken(claims);

  RpcHarness h;
  auto w = h.MakeWriter(false);
  w.SendHeaders(connect::kHttpOk, std::string(connect::kContentTypeProto));

  // Simulate writing a serialized protobuf containing the token
  std::vector<uint8_t> payload(token.begin(), token.end());
  w.WriteUnary(std::span<const uint8_t>(payload));

  EXPECT_EQ(h.StatusHeader(), "200");
  ASSERT_EQ(h.sent_body.size(), 1u);
  std::string body(h.sent_body[0].begin(), h.sent_body[0].end());
  EXPECT_EQ(body, token);
}

// ── Error response ────────────────────────────────────────────────────────────

TEST(UnaryRpcTest, ErrorResponseContainsCodeAndMessage) {
  RpcHarness h;
  auto w = h.MakeWriter(false);
  w.SendHeaders(connect::kHttpUnauthorized, "application/json");
  w.WriteError(std::string(connect::kCodeUnauthenticated), "invalid credentials");

  EXPECT_EQ(h.StatusHeader(), "401");
  ASSERT_EQ(h.sent_body.size(), 1u);
  std::string body(h.sent_body[0].begin(), h.sent_body[0].end());
  EXPECT_NE(body.find("unauthenticated"), std::string::npos);
  EXPECT_NE(body.find("invalid credentials"), std::string::npos);
}

// ── HttpStatusForConnectCode mapping ─────────────────────────────────────────

TEST(UnaryRpcTest, ConnectCodeHttpStatusMapping) {
  EXPECT_EQ(connect::HttpStatusForConnectCode(connect::kCodeOk),               200);
  EXPECT_EQ(connect::HttpStatusForConnectCode(connect::kCodeUnauthenticated),  401);
  EXPECT_EQ(connect::HttpStatusForConnectCode(connect::kCodePermissionDenied), 403);
  EXPECT_EQ(connect::HttpStatusForConnectCode(connect::kCodeNotFound),         404);
  EXPECT_EQ(connect::HttpStatusForConnectCode(connect::kCodeAlreadyExists),    409);
  EXPECT_EQ(connect::HttpStatusForConnectCode(connect::kCodeInvalidArgument),  400);
  EXPECT_EQ(connect::HttpStatusForConnectCode(connect::kCodeUnavailable),      503);
}

// ── Full unary flow: parse request, dispatch, write response ──────────────────

TEST(UnaryRpcTest, UnaryFlowWithValidPayload) {
  std::string fake_proto_body = "\x08\x01\x12\x05hello";  // arbitrary bytes
  connect::ParsedConnectRequest req;
  req.method       = "POST";
  req.path         = "/test.Service/Method";
  req.content_type = std::string(connect::kContentTypeProto);
  req.body.assign(fake_proto_body.begin(), fake_proto_body.end());

  RpcHarness h;
  auto w = h.MakeWriter(false);
  w.SendHeaders(connect::kHttpOk, std::string(connect::kContentTypeProto));
  w.WriteUnary(std::span<const uint8_t>(req.body));

  EXPECT_EQ(h.StatusHeader(), "200");
  ASSERT_EQ(h.sent_body.size(), 1u);
  EXPECT_EQ(h.sent_body[0].size(), fake_proto_body.size());
}

// ── ExtractBearerToken ────────────────────────────────────────────────────────

TEST(UnaryRpcTest, ExtractBearerTokenValid) {
  auto tok = connect::ExtractBearerToken("Bearer eyJhbGciOiJIUzI1NiJ9.payload.sig");
  EXPECT_EQ(tok, "eyJhbGciOiJIUzI1NiJ9.payload.sig");
}

TEST(UnaryRpcTest, ExtractBearerTokenMissingPrefix) {
  auto tok = connect::ExtractBearerToken("Basic dXNlcjpwYXNz");
  EXPECT_TRUE(tok.empty());
}

TEST(UnaryRpcTest, ExtractBearerTokenEmpty) {
  auto tok = connect::ExtractBearerToken("");
  EXPECT_TRUE(tok.empty());
}
