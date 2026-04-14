// FILE: buf_connect_server/src/connect/protocol.cpp
#include "buf_connect_server/connect/protocol.hpp"

int buf_connect_server::connect::HttpStatusForConnectCode(std::string_view code) {
    if (code == kCodeOk)               return kHttpOk;
    if (code == kCodeUnauthenticated)  return kHttpUnauthorized;
    if (code == kCodePermissionDenied) return kHttpForbidden;
    if (code == kCodeNotFound)         return kHttpNotFound;
    if (code == kCodeAlreadyExists)    return kHttpConflict;
    if (code == kCodeInvalidArgument)  return kHttpBadRequest;
    if (code == kCodeUnavailable)      return kHttpUnavailable;
    return kHttpInternalError;
}

bool buf_connect_server::connect::IsStreamingContentType(std::string_view ct) {
    return ct == kContentTypeConnectProto || ct == kContentTypeConnectJson
           || ct == kContentTypeGrpc        || ct == kContentTypeGrpcWebProto;
}
