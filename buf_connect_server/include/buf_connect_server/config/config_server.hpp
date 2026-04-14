// FILE: buf_connect_server/include/buf_connect_server/config/config_server.hpp
#ifndef BUF_CONNECT_SERVER_CONFIG_CONFIG_SERVER_HPP
#define BUF_CONNECT_SERVER_CONFIG_CONFIG_SERVER_HPP

#include "buf_connect_server/config/server_config.hpp"
#include "buf_connect_server/auth/user_store.hpp"
#include <functional>
#include <memory>
#include <string>

namespace httplib { class Server; }

namespace buf_connect_server {

using ConfigUpdateCallback = std::function<void(const ServerConfig&)>;

class ConfigServer {
 public:
  explicit ConfigServer(ServerConfig* config,
                        auth::UserStore* user_store,
                        const std::string& static_dir,
                        ConfigUpdateCallback on_update = nullptr);
  ~ConfigServer();

  void Start(const std::string& host, uint16_t port);
  void Stop();

  // Register a download token for archive file access
  void RegisterDownloadToken(const std::string& token,
                              const std::string& file_path,
                              uint32_t ttl_seconds);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace buf_connect_server

#endif  // BUF_CONNECT_SERVER_CONFIG_CONFIG_SERVER_HPP
