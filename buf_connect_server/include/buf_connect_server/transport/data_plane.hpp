// FILE: buf_connect_server/include/buf_connect_server/transport/control_plane.hpp
#ifndef BUF_CONNECT_SERVER_TRANSPORT_DATA_PLANE_HPP
#define BUF_CONNECT_SERVER_TRANSPORT_DATA_PLANE_HPP

#include "buf_connect_server/config/server_config.hpp"
#include "buf_connect_server/http2/router.hpp"
#include <memory>

namespace buf_connect_server::transport {

    class DataPlane {
    public:
        explicit DataPlane(const InterfaceConfig& config);
        ~DataPlane();

        http2::Http2Router& GetRouter();
        void Start();
        void Stop();

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}  // namespace buf_connect_server::transport

#endif  // BUF_CONNECT_SERVER_TRANSPORT_DATA_PLANE_HPP