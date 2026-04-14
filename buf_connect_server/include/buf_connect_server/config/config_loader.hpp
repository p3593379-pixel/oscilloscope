// FILE: buf_connect_server/include/buf_connect_server/config/config_loader.hpp
#ifndef BUF_CONNECT_SERVER_CONFIG_CONFIG_LOADER_HPP
#define BUF_CONNECT_SERVER_CONFIG_CONFIG_LOADER_HPP

#include "buf_connect_server/config/server_config.hpp"
#include <string>

namespace buf_connect_server {

class ConfigLoader {
 public:
  static ServerConfig LoadFromFile(const std::string& path);
  static void         SaveToFile(const ServerConfig& config, const std::string& path);
  static ServerConfig FromJson(const std::string& json_str);
  static std::string  ToJson(const ServerConfig& config);
};

}  // namespace buf_connect_server

#endif  // BUF_CONNECT_SERVER_CONFIG_CONFIG_LOADER_HPP
