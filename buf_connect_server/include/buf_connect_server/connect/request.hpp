// FILE: buf_connect_server/include/buf_connect_server/connect/request.hpp
#ifndef BUF_CONNECT_SERVER_CONNECT_REQUEST_HPP
#define BUF_CONNECT_SERVER_CONNECT_REQUEST_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace buf_connect_server::connect {

struct ParsedConnectRequest {
  std::string                                    path;
  std::string                                    method;
  std::string                                    content_type;
  std::unordered_map<std::string, std::string>   headers;
  std::vector<uint8_t>                           body;
  bool                                           is_streaming = false;
  std::string                                    bearer_token;
  std::string                                    connect_protocol_version;
};

// Extract Bearer token from Authorization header value.
std::string ExtractBearerToken(const std::string& authorization_header);
std::string ExtractAuthorizationBearer(const connect::ParsedConnectRequest& req);
std::vector<uint8_t> ExtractUnaryBody(const connect::ParsedConnectRequest& req);

}  // namespace buf_connect_server::connect

#endif  // BUF_CONNECT_SERVER_CONNECT_REQUEST_HPP
