// FILE: buf_connect_server/tests/config/test_config_loader.cpp
#include "buf_connect_server/config/config_loader.hpp"
#include <gtest/gtest.h>

using namespace buf_connect_server;

TEST(ConfigLoaderTest, RoundTripJson) {
ServerConfig c;
c.control_plane.port = 9090;
c.auth.access_token_ttl_seconds = 1800;
c.log.level = "debug";
auto json_str = ConfigLoader::ToJson(c);
auto loaded   = ConfigLoader::FromJson(json_str);
EXPECT_EQ(loaded.control_plane.port, 9090);
EXPECT_EQ(loaded.auth.access_token_ttl_seconds, 1800u);
EXPECT_EQ(loaded.log.level, "debug");
}

TEST(ConfigLoaderTest, DefaultsOnMissingFields) {
auto loaded = ConfigLoader::FromJson("{}");
EXPECT_EQ(loaded.control_plane.port, 8080);
EXPECT_EQ(loaded.auth.access_token_ttl_seconds, 900u);
EXPECT_FALSE(loaded.single_interface_mode);
}

TEST(ConfigLoaderTest, JwtSecretNotInOutput) {
ServerConfig c;
c.auth.jwt_secret = "very_secret";
auto json_str = ConfigLoader::ToJson(c);
EXPECT_EQ(json_str.find("very_secret"), std::string::npos);
}
