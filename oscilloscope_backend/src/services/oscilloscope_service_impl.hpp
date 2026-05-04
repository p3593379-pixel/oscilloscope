#ifndef OSCILLOSCOPE_BACKEND_SERVICES_OSCILLOSCOPE_SERVICE_IMPL_HPP
#define OSCILLOSCOPE_BACKEND_SERVICES_OSCILLOSCOPE_SERVICE_IMPL_HPP

#include "buf_connect_server/server.hpp"
#include "buf_connect_server/auth/stream_token.hpp"
#include <mutex>
#include <string>

class OscilloscopeServiceImpl : public buf_connect_server::ServiceHandlerBase {
public:
    // jwt_secret is forwarded from the ServerConfig so the data plane can
    // validate stream tokens without a database lookup.
    explicit OscilloscopeServiceImpl(std::string jwt_secret = {});

    std::string ServicePath() const override;
    void RegisterRoutes(buf_connect_server::BufConnectServer& server) override;

private:
    void HandleStreamData(const buf_connect_server::connect::ParsedConnectRequest&,
                          buf_connect_server::connect::ConnectResponseWriter&);
    void HandleGetSettings(const buf_connect_server::connect::ParsedConnectRequest&,
                           buf_connect_server::connect::ConnectResponseWriter&);
    void HandleUpdateSettings(const buf_connect_server::connect::ParsedConnectRequest&,
                              buf_connect_server::connect::ConnectResponseWriter&);

    // Passed in at construction time — used only to derive the HMAC key
    buf_connect_server::auth::StreamToken stream_token_;

    // Settings are written by UpdateSettings and read by the streaming loop
    mutable std::mutex settings_mutex_;
    uint32_t sample_rate_hz_   = 250000000;
    uint32_t channels_         = 4;
    uint32_t voltage_range_mv_ = 1000;
    uint32_t trigger_level_mv_ = 0;
    bool     trigger_enabled_  = false;
};

#endif  // OSCILLOSCOPE_BACKEND_SERVICES_OSCILLOSCOPE_SERVICE_IMPL_HPP
