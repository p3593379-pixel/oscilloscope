// FILE: buf_connect_server/src/auth/user_store.cpp
#include "buf_connect_server/auth/user_store.hpp"
#include "spdlog/spdlog.h"
#include <sqlite_orm/sqlite_orm.h>
#include <argon2.h>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>

using namespace sqlite_orm;

namespace {

    std::string GenerateUuid() {
        std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<uint64_t> dist;
        std::ostringstream oss;
        auto v1 = dist(rng), v2 = dist(rng);
        oss << std::hex << std::setfill('0')
            << std::setw(8) << (v1 >> 32) << "-"
            << std::setw(4) << ((v1 >> 16) & 0xFFFF) << "-"
            << std::setw(4) << (v1 & 0xFFFF) << "-"
            << std::setw(4) << (v2 >> 48) << "-"
            << std::setw(12) << (v2 & 0xFFFFFFFFFFFFull);
        return oss.str();
    }

    std::string HashPassword(const std::string& password) {
        const uint32_t t_cost = 2, m_cost = 65536, parallelism = 1;
        char encoded[256];
        uint8_t salt[16];
        std::mt19937 rng(std::random_device{}());
        for (auto& b : salt) b = static_cast<uint8_t>(rng());
        int rc = argon2id_hash_encoded(
                t_cost, m_cost, parallelism,
                password.data(), password.size(),
                salt, sizeof(salt),
                32, encoded, sizeof(encoded));
        if (rc != ARGON2_OK) throw std::runtime_error("argon2 hashing failed");
        return std::string(encoded);
    }

    bool VerifyHash(const std::string& hash, const std::string& password) {
        return argon2id_verify(hash.c_str(), password.data(), password.size()) == ARGON2_OK;
    }

    auto MakeStorage(const std::string& db_path) {
        using namespace sqlite_orm;
        return make_storage(
                db_path,
                make_table("users",
                           make_column("id",            &buf_connect_server::auth::UserRecord::id,
                                       primary_key()),
                           make_column("username",      &buf_connect_server::auth::UserRecord::username,
                                       unique()),
                           make_column("password_hash", &buf_connect_server::auth::UserRecord::password_hash),
                           make_column("role",          &buf_connect_server::auth::UserRecord::role),
                           make_column("created_at",    &buf_connect_server::auth::UserRecord::created_at),
                           make_column("last_login",    &buf_connect_server::auth::UserRecord::last_login)
                )
        );
    }

    using StorageType = decltype(MakeStorage(""));

}  // namespace

class buf_connect_server::auth::UserStore::Impl {
public:
    explicit Impl(const std::string& db_path)
            : storage_(MakeStorage(db_path)) {
        storage_.sync_schema();
    }
    StorageType storage_;
};

buf_connect_server::auth::UserStore::UserStore(const std::string& db_path)
        : impl_(std::make_unique<Impl>(db_path))
        {
            int a = 0;
            SPDLOG_DEBUG("a = {}", a);
        }

buf_connect_server::auth::UserStore::~UserStore() = default;

std::optional<buf_connect_server::auth::UserRecord>
buf_connect_server::auth::UserStore::CreateUser(
        const std::string& username,
        const std::string& password,
        const std::string& role) {
    auto existing = FindByUsername(username);
    if (existing) return std::nullopt;
    UserRecord rec;
    rec.id            = GenerateUuid();
    rec.username      = username;
    rec.password_hash = HashPassword(password);
    rec.role          = role;
    rec.created_at    = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
    rec.last_login    = 0;
    impl_->storage_.replace(rec);
    return rec;
}

std::optional<buf_connect_server::auth::UserRecord>
buf_connect_server::auth::UserStore::FindByUsername(const std::string& username) const {
    auto results = impl_->storage_.get_all<UserRecord>(
            where(c(&UserRecord::username) == username));
    if (results.empty()) return std::nullopt;
    return results.front();
}

std::optional<buf_connect_server::auth::UserRecord>
buf_connect_server::auth::UserStore::FindById(const std::string& id) const {
    try {
        return impl_->storage_.get<UserRecord>(id);
    } catch (...) {
        return std::nullopt;
    }
}

bool buf_connect_server::auth::UserStore::VerifyPassword(
        const std::string& user_id, const std::string& password) const {
    auto rec = FindById(user_id);
    if (!rec) return false;
    return VerifyHash(rec->password_hash, password);
}

bool buf_connect_server::auth::UserStore::DeleteUser(const std::string& user_id) {
    try {
        if (!FindById(user_id).has_value())
            return false;
        impl_->storage_.remove<UserRecord>(user_id);
        return true;
    } catch (...) {
        return false;
    }
}

bool buf_connect_server::auth::UserStore::ResetPassword(
        const std::string& user_id, const std::string& new_password) {
    auto rec = FindById(user_id);
    if (!rec) return false;
    rec->password_hash = HashPassword(new_password);
    impl_->storage_.update(*rec);
    return true;
}

bool buf_connect_server::auth::UserStore::UpdateLastLogin(const std::string& user_id) {
    auto rec = FindById(user_id);
    if (!rec) return false;
    rec->last_login = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
    impl_->storage_.update(*rec);
    return true;
}

std::vector<buf_connect_server::auth::UserRecord>
buf_connect_server::auth::UserStore::ListUsers() const {
    return impl_->storage_.get_all<UserRecord>();
}
