#ifndef BUF_CONNECT_SERVER_CONFIG_NGINX_WRITER_HPP
#define BUF_CONNECT_SERVER_CONFIG_NGINX_WRITER_HPP

#include "buf_connect_server/config/server_config.hpp"
#include <string>
#include <filesystem>

namespace buf_connect_server {

/**
 * Generates and hot-applies the oscilloscope nginx config from InterfaceConfig.
 *
 * Controlled by control_panel_web:
 *   - control_plane / data_plane: bind_address, port, tls (cert, key, min_version)
 *   - data_plane.enabled: false → merge data location block into the control server{}
 *
 * Never changes (hardcoded):
 *   - upstream addresses  127.0.0.3:1254 / 127.0.0.3:1256
 *   - location regex patterns and all proxy_* directives
 *   - HTTP→HTTPS redirect block
 *
 * Permissions: see nginx_writer.cpp top comment.
 */
    class NginxWriter {
    public:
        struct Options {
            std::filesystem::path conf_path         = "/etc/nginx/conf.d/oscilloscope.conf";
            std::string           server_name       = "my_oscilloscope";
            std::string           static_root       = "./static/oscilloscope_web";
            std::string           nginx_reload_cmd  = "sudo /usr/sbin/nginx -s reload";

            // Internal h2c endpoints — never exposed to the outer network
            std::string control_upstream_addr = "127.0.0.3:1254";
            std::string data_upstream_addr    = "127.0.0.3:1256";
        };

        explicit NginxWriter(Options opts);

        /**
         * Write the generated config and reload nginx.
         * Returns empty string on success, error description on failure.
         */
        std::string Apply(const InterfaceConfig& cp, const InterfaceConfig& dp);

        /** Generate config string without writing — useful for tests / preview. */
        std::string Generate(const InterfaceConfig& cp, const InterfaceConfig& dp) const;

    private:
        Options opts_;

        std::string MakeTlsBlock(const TlsConfig& tls) const;
        std::string MakeStaticLocation() const;
        std::string MakeServerBlock(const InterfaceConfig& iface,
                                    const std::string& locations) const;
        std::string MakeUpstreams() const;
        std::string MakeHttpRedirect(const InterfaceConfig& cp,
                                     const InterfaceConfig& dp) const;
    };

} // namespace buf_connect_server

#endif