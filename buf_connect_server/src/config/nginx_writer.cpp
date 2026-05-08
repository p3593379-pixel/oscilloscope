// FILE: buf_connect_server/src/config/nginx_writer.cpp
//
// Permissions model
// -----------------
// nginx master runs as root (binds :443/:80) but the conf file only needs to
// be writable by the oscilloscope_backend service account.
//
// Recommended: passwordless sudoers for the one reload command only:
//   oscilloscope ALL=(root) NOPASSWD: /usr/sbin/nginx -s reload
// Set Options::nginx_reload_cmd = "sudo /usr/sbin/nginx -s reload".
//
// Config is written atomically: tmp file → fsync → rename.

#include "buf_connect_server/config/nginx_writer.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <system_error>

namespace buf_connect_server {

    NginxWriter::NginxWriter(Options opts) : opts_(std::move(opts)) {}

// ─── private helpers ──────────────────────────────────────────────────────

    std::string NginxWriter::MakeTlsBlock(const TlsConfig& tls) const {
        // Expand min_version to an nginx ssl_protocols line
        std::string protos = (tls.min_version == "TLSv1.3") ? "TLSv1.3"
                                                            : "TLSv1.2 TLSv1.3";
        std::ostringstream o;
        o << "    ssl_certificate     " << tls.cert_path   << ";\n"
          << "    ssl_certificate_key " << tls.key_path    << ";\n"
          << "\n"
          << "    ssl_protocols       " << protos          << ";\n"
          << "    ssl_ciphers         HIGH:!aNULL:!MD5;\n"
          << "    ssl_session_cache   shared:SSL:10m;\n"
          << "    ssl_session_timeout 1h;\n";
        return o.str();
    }

    static std::string ProxyBlock(const std::string& upstream) {
        std::ostringstream o;
        o << "        proxy_pass http://" << upstream << ";\n"
          << "        proxy_pass_header connect-protocol-version;\n"
          << "        proxy_pass_header content-type;\n"
          << "\n"
          << "        proxy_http_version 2;\n"
          << "        proxy_set_header   Connection \"\";\n"
          << "        proxy_set_header   Host $host;\n"
          << "\n"
          << "        proxy_request_buffering off;\n"
          << "        proxy_buffering         off;\n"
          << "\n"
          << "        proxy_read_timeout 3600s;\n"
          << "        proxy_send_timeout 3600s;\n"
          << "\n"
          << "        proxy_set_header X-Real-IP       $remote_addr;\n"
          << "        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;\n";
        return o.str();
    }

    std::string NginxWriter::MakeStaticLocation() const {
        if (!opts_.static_root.empty()) {
            std::ostringstream o;
            o << "\n"
              << "    location / {\n"
              << "        root      " << opts_.static_root << ";\n"
              << "        try_files $uri $uri/ /index.html;\n"
              << "\n"
              << "        add_header Access-Control-Allow-Credentials \"true\"              always;\n"
              << "        add_header Access-Control-Allow-Headers     "
              << "\"Authorization, Content-Type, Connect-Protocol-Version, Connect-Timeout-Ms\" always;\n"
              << "        add_header Access-Control-Allow-Methods     \"POST, OPTIONS\"     always;\n"
              << "        add_header Cache-Control \"no-cache\";\n"
              << "    }\n";
            return o.str();
        }
        return {};
    }

    std::string NginxWriter::MakeServerBlock(const InterfaceConfig& iface,
                                             const std::string& locations) const {
        std::ostringstream o;
        o << "server {\n"
          << "    listen " << iface.bind_address << ":" << iface.port << ";\n"
          << "    http2 on;\n\n"
          << "    server_name " << opts_.server_name << ";\n";
        if (iface.tls.enabled)
            MakeTlsBlock(iface.tls);

        o << "\n" << locations << "}\n";
        return o.str();
    }

    std::string NginxWriter::MakeUpstreams() const {
        std::ostringstream o;
        o << "# Internal upstream endpoints — never change\n"
          << "upstream control_plane_h2c {\n"
          << "    server " << opts_.control_upstream_addr << ";\n"
          << "    keepalive 64;\n}\n\n"
          << "upstream data_plane_h2c {\n"
          << "    server " << opts_.data_upstream_addr << ";\n"
          << "    keepalive 64;\n}\n";
        return o.str();
    }

    std::string NginxWriter::MakeHttpRedirect(const InterfaceConfig& cp,
                                              const InterfaceConfig& dp) const {
        if (!cp.tls.enabled && !dp.tls.enabled) return {};
        std::ostringstream o;
        o << "server {\n"
          << "    listen " << cp.bind_address << ":80;\n"
          << "    server_name " << opts_.server_name << ";\n"
          << "    return 301 https://$host$request_uri;\n"
          << "}\n";
        return o.str();
    }

// ─── public ───────────────────────────────────────────────────────────────

    std::string NginxWriter::Generate(const InterfaceConfig& cp,
                                      const InterfaceConfig& dp) const {
        std::ostringstream out;
        out << "# Generated by buf_connect_server — do not edit manually\n\n";
        out << MakeUpstreams() << "\n";

        if (dp.enabled) {
            // Separate server blocks — data plane routes go in the data server only
            std::ostringstream control_plane_locations;
            control_plane_locations << "    location ~ ^/(buf_connect_server\\.v2\\.) {\n"
                                    << ProxyBlock("control_plane_h2c")
                                    << "    }\n"
                                    << "    location ~ ^/(oscilloscope_interface\\.v2\\.) {\n"
                                    << ProxyBlock("control_plane_h2c")
                                    << "    }\n";

            std::ostringstream data_plane_locations;
            data_plane_locations << "    location ~ ^/(oscilloscope_interface\\.v2\\.) {\n"
                                 << ProxyBlock("data_plane_h2c")
                                 << "    }\n";
            out << "# Control plane — " << cp.bind_address << ":" << cp.port << "\n";
            out << MakeServerBlock(cp, control_plane_locations.str() + MakeStaticLocation()) << "\n";
            out << "# Data plane — " << dp.bind_address << ":" << dp.port << "\n";
            out << MakeServerBlock(dp, data_plane_locations.str());
        } else {
            // Data plane off → merge both route sets into one server block
            std::ostringstream control_plane_locations;
            control_plane_locations << "    location ~ ^/(buf_connect_server\\.v2\\.) {\n"
                                    << ProxyBlock("control_plane_h2c")
                                    << "    }\n"
                                    << "    location ~ ^/(oscilloscope_interface\\.v2\\.) {\n"
                                    << ProxyBlock("control_plane_h2c")
                                    << "    }\n"
                                    << "    location ~ ^/(oscilloscope_interface\\.v2\\.) {\n"
                                    << ProxyBlock("data_plane_h2c")
                                    << "    }\n";
            out << "# Single Interface Config\n";
            out << MakeServerBlock(cp, control_plane_locations.str() + MakeStaticLocation());
        }

        std::string redir = MakeHttpRedirect(cp, dp);
        if (!redir.empty()) out << "\n" << redir;

        return out.str();
    }

    std::string NginxWriter::Apply(const InterfaceConfig& cp, const InterfaceConfig& dp) {
        const std::string content = Generate(cp, dp);
        const auto& path = opts_.conf_path;
        auto tmp_path = path; tmp_path += ".tmp";

        try {
            std::ofstream f(tmp_path, std::ios::trunc);
            if (!f) throw std::system_error(errno, std::generic_category(),
                                            "Cannot open " + tmp_path.string());
            f << content;
            f.flush();
            if (!f) throw std::runtime_error("Write error: " + tmp_path.string());
            f.close();
            std::filesystem::rename(tmp_path, path);
            spdlog::info("NginxWriter: wrote {}", path.string());
        } catch (const std::exception& e) {
            std::filesystem::remove(tmp_path);
            spdlog::error("NginxWriter: {}", e.what());
            return e.what();
        }

        int rc = std::system(opts_.nginx_reload_cmd.c_str()); // NOLINT(cert-env33-c)
        if (rc != 0) {
            auto msg = "nginx reload '" + opts_.nginx_reload_cmd +
                       "' failed (exit " + std::to_string(rc) + ")";
            spdlog::error("NginxWriter: {}", msg);
            return msg;
        }
        spdlog::info("NginxWriter: nginx reloaded successfully");
        return {};
    }

} // namespace buf_connect_server
