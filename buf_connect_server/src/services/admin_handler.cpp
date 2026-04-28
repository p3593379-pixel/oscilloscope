#include "buf_connect_server/services/admin_handler.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include "buf_connect_server/connect/frame_codec.hpp"
#include "buf_connect_server.pb.h"
#include <spdlog/spdlog.h>

namespace buf_connect_server::services {

    AdminHandler::AdminHandler(auth::UserStore&  user_store,
                               const AuthConfig& auth_config)
            : user_store_(user_store) {
        auth_middleware_ = std::make_shared<auth::AuthMiddleware>(
                std::make_shared<auth::JwtIssuer>(auth_config.jwt_secret));
    }

    std::string AdminHandler::ServicePath() const {
        return "/buf_connect_server.v2.AdminService";
    }

    void AdminHandler::RegisterRoutes(BufConnectServer& server) {
        server.RegisterControlRoute(
                "/buf_connect_server.v2.AdminService/ListUsers",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) { HandleListUsers(req, w); });
        server.RegisterControlRoute(
                "/buf_connect_server.v2.AdminService/CreateUser",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) { HandleCreateUser(req, w); });
        server.RegisterControlRoute(
                "/buf_connect_server.v2.AdminService/DeleteUser",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) { HandleDeleteUser(req, w); });
        server.RegisterControlRoute(
                "/buf_connect_server.v2.AdminService/ResetPassword",
                [this](const connect::ParsedConnectRequest& req,
                       connect::ConnectResponseWriter& w) { HandleResetPassword(req, w); });
    }

// Returns false and writes a 403 if the caller is not an admin.
    bool AdminHandler::RequireAdmin(const connect::ParsedConnectRequest& req,
                                    connect::ConnectResponseWriter& w) const {
        namespace c = connect;
        auto auth_it = req.headers.find("authorization");
        if (auth_it == req.headers.end()) {
            w.SendHeaders(c::kHttpUnauthorized, "application/json");
            w.WriteError(std::string(c::kCodeUnauthenticated),
                         "missing Authorization header");
            return false;
        }
        auto ctx = auth_middleware_->Authenticate(auth_it->second);
        if (!ctx || ctx->role != "admin") {
            w.SendHeaders(c::kHttpForbidden, "application/json");
            w.WriteError(std::string(c::kCodePermissionDenied), "admin role required");
            return false;
        }
        return true;
    }

    void AdminHandler::HandleListUsers(const connect::ParsedConnectRequest& req,
                                       connect::ConnectResponseWriter& w) {
        namespace c = connect;
        if (!RequireAdmin(req, w)) return;

        auto users = user_store_.ListUsers();
        v2::ListUsersResponse resp;
        for (const auto& u : users) {
            auto* info = resp.add_users();
            info->set_user_id(u.id);
            info->set_username(u.username);
            info->set_role(u.role == "admin"
                           ? v2::USER_ROLE_ADMIN
                           : v2::USER_ROLE_ENGINEER);
            info->set_created_at(u.created_at);
            info->set_last_login(u.last_login);
        }
        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
        w.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                            std::span<const uint8_t>(out));
    }

    void AdminHandler::HandleCreateUser(const connect::ParsedConnectRequest& req,
                                        connect::ConnectResponseWriter& w) {
        namespace c = connect;
        if (!RequireAdmin(req, w)) return;

        v2::CreateUserRequest create_req;
        if (!create_req.ParseFromArray(req.body.data(),
                                       static_cast<int>(req.body.size()))) {
            w.SendHeaders(c::kHttpBadRequest, "application/json");
            w.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
            return;
        }
        std::string role = (create_req.role() == v2::USER_ROLE_ADMIN)
                           ? "admin" : "engineer";
        auto user = user_store_.CreateUser(create_req.username(),
                                           create_req.password(), role);
        if (!user) {
            w.SendHeaders(c::kHttpConflict, "application/json");
            w.WriteError(std::string(c::kCodeAlreadyExists), "username already exists");
            return;
        }
        v2::CreateUserResponse resp;
        auto* info = resp.mutable_user();
        info->set_user_id(user->id);
        info->set_username(user->username);
        info->set_role(create_req.role());
        info->set_created_at(user->created_at);
        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
        w.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                            std::span<const uint8_t>(out));
    }

    void AdminHandler::HandleDeleteUser(const connect::ParsedConnectRequest& req,
                                        connect::ConnectResponseWriter& w) {
        namespace c = connect;
        if (!RequireAdmin(req, w)) return;

        v2::DeleteUserRequest del_req;
        if (!del_req.ParseFromArray(req.body.data(),
                                    static_cast<int>(req.body.size()))) {
            w.SendHeaders(c::kHttpBadRequest, "application/json");
            w.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
            return;
        }
        bool ok = user_store_.DeleteUser(del_req.user_id());
        v2::DeleteUserResponse resp;
        resp.set_success(ok);
        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
        w.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                            std::span<const uint8_t>(out));
    }

    void AdminHandler::HandleResetPassword(const connect::ParsedConnectRequest& req,
                                           connect::ConnectResponseWriter& w) {
        namespace c = connect;
        if (!RequireAdmin(req, w)) return;

        v2::ResetPasswordRequest reset_req;
        if (!reset_req.ParseFromArray(req.body.data(),
                                      static_cast<int>(req.body.size()))) {
            w.SendHeaders(c::kHttpBadRequest, "application/json");
            w.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
            return;
        }
        bool ok = user_store_.ResetPassword(reset_req.user_id(),
                                            reset_req.new_password());
        v2::ResetPasswordResponse resp;
        resp.set_success(ok);
        std::vector<uint8_t> out(resp.ByteSizeLong());
        resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
        w.SendUnaryResponse(c::kHttpOk, c::kContentTypeProto,
                            std::span<const uint8_t>(out));
    }

}  // namespace buf_connect_server::services
