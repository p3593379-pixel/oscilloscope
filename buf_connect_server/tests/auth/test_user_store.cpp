// FILE: buf_connect_server/tests/auth/test_user_store.cpp
#include "buf_connect_server/auth/user_store.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <string>

using namespace buf_connect_server::auth;

class UserStoreTest : public ::testing::Test {
protected:
    std::string db_path_;
    std::unique_ptr<UserStore> store_;

    void SetUp() override {
        db_path_ = std::filesystem::temp_directory_path() /
                   ("test_users_" + std::to_string(std::time(nullptr)) + ".db");
        store_ = std::make_unique<UserStore>(db_path_);
    }

    void TearDown() override {
        store_.reset();
        std::filesystem::remove(db_path_);
    }
};

TEST_F(UserStoreTest, CreateAndFindByUsername) {
    auto u = store_->CreateUser("alice", "secret123", "engineer");
    ASSERT_TRUE(u.has_value());
    EXPECT_EQ(u->username, "alice");
    EXPECT_EQ(u->role,     "engineer");
    EXPECT_FALSE(u->id.empty());

    auto found = store_->FindByUsername("alice");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, u->id);
}

TEST_F(UserStoreTest, FindByIdWorks) {
    auto u = store_->CreateUser("bob", "pass", "admin");
    ASSERT_TRUE(u.has_value());
    auto found = store_->FindById(u->id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->username, "bob");
}

TEST_F(UserStoreTest, DuplicateUsernameReturnsNullopt) {
    store_->CreateUser("carol", "pass1", "engineer");
    auto dup = store_->CreateUser("carol", "pass2", "admin");
    EXPECT_FALSE(dup.has_value());
}

TEST_F(UserStoreTest, VerifyPasswordCorrect) {
    auto u = store_->CreateUser("dave", "correct_horse", "engineer");
    ASSERT_TRUE(u.has_value());
    EXPECT_TRUE(store_->VerifyPassword(u->id, "correct_horse"));
}

TEST_F(UserStoreTest, VerifyPasswordIncorrect) {
    auto u = store_->CreateUser("eve", "real_password", "engineer");
    ASSERT_TRUE(u.has_value());
    EXPECT_FALSE(store_->VerifyPassword(u->id, "wrong_password"));
}

TEST_F(UserStoreTest, ResetPassword) {
    auto u = store_->CreateUser("frank", "old_pass", "engineer");
    ASSERT_TRUE(u.has_value());
    EXPECT_TRUE(store_->ResetPassword(u->id, "new_pass"));
    EXPECT_TRUE(store_->VerifyPassword(u->id, "new_pass"));
    EXPECT_FALSE(store_->VerifyPassword(u->id, "old_pass"));
}

TEST_F(UserStoreTest, DeleteUser) {
    auto u = store_->CreateUser("grace", "pass", "admin");
    ASSERT_TRUE(u.has_value());
    EXPECT_TRUE(store_->DeleteUser(u->id));
    EXPECT_FALSE(store_->FindById(u->id).has_value());
}

TEST_F(UserStoreTest, DeleteNonExistentReturnsFalse) {
    EXPECT_FALSE(store_->DeleteUser("nonexistent-uuid-1234"));
}

TEST_F(UserStoreTest, ListUsersReturnsAll) {
    store_->CreateUser("u1", "p", "engineer");
    store_->CreateUser("u2", "p", "admin");
    store_->CreateUser("u3", "p", "engineer");
    auto all = store_->ListUsers();
    EXPECT_EQ(all.size(), 3u);
}

TEST_F(UserStoreTest, UpdateLastLogin) {
    auto u = store_->CreateUser("henry", "pass", "engineer");
    ASSERT_TRUE(u.has_value());
    EXPECT_EQ(u->last_login, 0);
    EXPECT_TRUE(store_->UpdateLastLogin(u->id));
    auto updated = store_->FindById(u->id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_GT(updated->last_login, 0);
}

TEST_F(UserStoreTest, FindByUsernameNotFound) {
    EXPECT_FALSE(store_->FindByUsername("nobody").has_value());
}
