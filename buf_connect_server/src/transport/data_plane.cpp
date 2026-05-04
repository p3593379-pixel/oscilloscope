// FILE: buf_connect_server/src/transport/data_plane.cpp
#include "buf_connect_server/transport/data_plane.hpp"
#include "buf_connect_server/http2/listener.hpp"
#include "buf_connect_server/http2/router.hpp"
#include <spdlog/spdlog.h>
#include <memory>

class buf_connect_server::transport::DataPlane::Impl {
 public:
  http2::Http2Router                    router_;
  std::unique_ptr<http2::Http2Listener> listener_;
  InterfaceConfig                       config_;

  explicit Impl(const InterfaceConfig& cfg) : config_(cfg)
  {
    listener_ = std::make_unique<http2::Http2Listener>(
        cfg.bind_address, cfg.port,
        cfg.tls.enabled, cfg.tls.cert_path, cfg.tls.key_path);
    listener_->SetRequestHandler(
        [this](const connect::ParsedConnectRequest& req,
               connect::ConnectResponseWriter& w) {
          router_.Dispatch(req, w);
        });
  }
};

buf_connect_server::transport::DataPlane::DataPlane(const InterfaceConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

buf_connect_server::transport::DataPlane::~DataPlane() { Stop(); }

buf_connect_server::http2::Http2Router&
buf_connect_server::transport::DataPlane::GetRouter() {
  return impl_->router_;
}

void buf_connect_server::transport::DataPlane::Start() {
  spdlog::info("DataPlane starting on {}:{}", impl_->config_.bind_address,
               impl_->config_.port);
  impl_->listener_->Start();
}

void buf_connect_server::transport::DataPlane::Stop() {
  impl_->listener_->Stop();
}
