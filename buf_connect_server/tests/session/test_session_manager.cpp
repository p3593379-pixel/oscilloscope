// FILE: buf_connect_server/tests/session/test_session_manager.cpp
#include "buf_connect_server/session/session_manager.hpp"
#include <gtest/gtest.h>

using namespace buf_connect_server::session;

TEST(SessionManagerTest, EngineerConnectsNoAdmin) {
SessionManager mgr;
auto mode = mgr.CreateSession("conn-1", "eng-1", "engineer");
EXPECT_EQ(mode, SessionMode::kActive);
}

TEST(SessionManagerTest, SecondEngineerBecomesObserver) {
SessionManager mgr;
    mgr.CreateSession("conn-1", "eng-1", "engineer");
auto mode = mgr.CreateSession("conn-2", "eng-2", "engineer");
EXPECT_EQ(mode, SessionMode::kObserver);
}

TEST(SessionManagerTest, AdminConnectsNoAdmin) {
SessionManager mgr;
auto mode = mgr.CreateSession("conn-1", "admin-1", "admin");
EXPECT_EQ(mode, SessionMode::kActiveAdmin);
}

TEST(SessionManagerTest, EngineerConnectsWhenAdminPresent) {
SessionManager mgr;
    mgr.CreateSession("conn-admin", "admin-1", "admin");
auto mode = mgr.CreateSession("conn-eng", "eng-1", "engineer");
EXPECT_EQ(mode, SessionMode::kOnService);
}

TEST(SessionManagerTest, ClaimActiveRoleSuccess) {
SessionManager mgr;
    mgr.CreateSession("conn-1", "eng-1", "engineer");
    mgr.CreateSession("conn-2", "eng-2", "engineer");
// conn-1 has ACTIVE, conn-2 has OBSERVER
// Disconnect conn-1 to free the role
mgr.Disconnect("conn-1");
auto result = mgr.ClaimActiveRole("conn-2");
EXPECT_TRUE(result.accepted);
}

TEST(SessionManagerTest, ClaimActiveRoleFailsWhenTaken) {
SessionManager mgr;
    mgr.CreateSession("conn-1", "eng-1", "engineer");  // ACTIVE
    mgr.CreateSession("conn-2", "eng-2", "engineer");  // OBSERVER
auto result = mgr.ClaimActiveRole("conn-2");
EXPECT_FALSE(result.accepted);
}
