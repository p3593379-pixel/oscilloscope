#ifndef BUF_CONNECT_SERVER_AUTH_USER_STORE_HPP
#define BUF_CONNECT_SERVER_AUTH_USER_STORE_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace buf_connect_server::auth {

    struct UserRecord {
        std::string id;
        std::string username;
        std::string password_hash;
        std::string role;  // "admin" | "engineer"
        int64_t     created_at  = 0;
        int64_t     last_login  = 0;
    };

    class UserStore {
    public:
        explicit UserStore(const std::string& db_path);
        ~UserStore();

        // Returns nullopt if username already exists.
        std::optional<UserRecord> CreateUser(const std::string& username,
                                             const std::string& password,
                                             const std::string& role);

        std::optional<UserRecord> FindByUsername(const std::string& username) const;
        std::optional<UserRecord> FindById(const std::string& id) const;

        // Returns true if password matches stored Argon2id hash.
        bool VerifyPassword(const std::string& user_id, const std::string& password) const;

        bool DeleteUser(const std::string& user_id);
        bool ResetPassword(const std::string& user_id, const std::string& new_password);
        bool UpdateLastLogin(const std::string& user_id);
        std::vector<UserRecord> ListUsers() const;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}  // namespace buf_connect_server::auth

#endif  // BUF_CONNECT_SERVER_AUTH_USER_STORE_HPP