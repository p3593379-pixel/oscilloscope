#ifndef BUF_CONNECT_SERVER_HTTP2_ROUTER_HPP
#define BUF_CONNECT_SERVER_HTTP2_ROUTER_HPP

#include "buf_connect_server/connect/request.hpp"
#include "buf_connect_server/connect/response_writer.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace buf_connect_server::http2 {

    using RouteHandler = std::function<void(
            const connect::ParsedConnectRequest&,
            connect::ConnectResponseWriter&)>;

    class Http2Router {
    public:
        Http2Router();

        void Register(const std::string& path, RouteHandler handler);
        void Dispatch(const connect::ParsedConnectRequest& req,
                      connect::ConnectResponseWriter& writer) const;

    private:
        std::unordered_map<std::string, RouteHandler> routes_;
    };

}  // namespace buf_connect_server::http2

#endif  // BUF_CONNECT_SERVER_HTTP2_ROUTER_HPP
