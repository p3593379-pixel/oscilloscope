#ifndef BUF_CONNECT_SERVER_HTTP2_LISTENER_HPP
#define BUF_CONNECT_SERVER_HTTP2_LISTENER_HPP

#include <functional>
#include <memory>
#include <string>

namespace buf_connect_server::connect {
    struct ParsedConnectRequest;
    class ConnectResponseWriter;
}

namespace buf_connect_server::http2 {

    using RequestHandler = std::function<void(
            const connect::ParsedConnectRequest&,
            connect::ConnectResponseWriter&)>;

    class Http2Listener {
    public:
        Http2Listener(const std::string& host, uint16_t port,
                      bool tls_enabled,
                      const std::string& cert_path,
                      const std::string& key_path);
        ~Http2Listener();

        void SetRequestHandler(RequestHandler handler);
        void Start();
        void Stop();

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}  // namespace buf_connect_server::http2

#endif  // BUF_CONNECT_SERVER_HTTP2_LISTENER_HPP