#ifndef BUF_CONNECT_SERVER_CONNECT_PROTOCOL_HPP
#define BUF_CONNECT_SERVER_CONNECT_PROTOCOL_HPP

#include <string_view>

namespace buf_connect_server::connect {

// Content-type strings
    inline constexpr std::string_view kContentTypeProto        = "application/proto";
    inline constexpr std::string_view kContentTypeConnectProto = "application/connect+proto";
    inline constexpr std::string_view kContentTypeJson         = "application/json";
    inline constexpr std::string_view kContentTypeConnectJson  = "application/connect+json";
    inline constexpr std::string_view kContentTypeGrpc         = "application/grpc";
    inline constexpr std::string_view kContentTypeGrpcWebProto = "application/grpc-web+proto";

// Connect error code strings
    inline constexpr std::string_view kCodeOk               = "ok";
    inline constexpr std::string_view kCodeUnauthenticated  = "unauthenticated";
    inline constexpr std::string_view kCodePermissionDenied = "permission_denied";
    inline constexpr std::string_view kCodeNotFound         = "not_found";
    inline constexpr std::string_view kCodeAlreadyExists    = "already_exists";
    inline constexpr std::string_view kCodeInvalidArgument  = "invalid_argument";
    inline constexpr std::string_view kCodeInternal         = "internal";
    inline constexpr std::string_view kCodeUnavailable      = "unavailable";

// HTTP status mappings
    inline constexpr int kHttpOk               = 200;
    inline constexpr int kHttpBadRequest       = 400;
    inline constexpr int kHttpUnauthorized     = 401;
    inline constexpr int kHttpForbidden        = 403;
    inline constexpr int kHttpNotFound         = 404;
    inline constexpr int kHttpConflict         = 409;
    inline constexpr int kHttpInternalError    = 500;
    inline constexpr int kHttpUnavailable      = 503;

// Connect protocol header
    inline constexpr std::string_view kConnectProtocolVersion = "1";
    inline constexpr std::string_view kConnectProtocolHeader  = "connect-protocol-version";

// Returns HTTP status code for a Connect error code string
    int HttpStatusForConnectCode(std::string_view code);

// Returns true if the content-type indicates a streaming request
    bool IsStreamingContentType(std::string_view content_type);

}  // namespace buf_connect_server::connect

#endif  // BUF_CONNECT_SERVER_CONNECT_PROTOCOL_HPP
