// FILE: buf_connect_server/src/http2/listener.cpp
#include "buf_connect_server/http2/listener.hpp"
#include "buf_connect_server/connect/request.hpp"
#include "buf_connect_server/connect/response_writer.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// Per-stream accumulator
struct StreamData {
  buf_connect_server::connect::ParsedConnectRequest request;
  std::vector<uint8_t>                              body_buf;
};

// Per-connection session
struct SessionContext {
  nghttp2_session*                               session  = nullptr;
  int                                            fd       = -1;
  SSL*                                           ssl      = nullptr;
  buf_connect_server::http2::RequestHandler      handler;
  std::unordered_map<int32_t, StreamData>        streams;
  std::mutex                                     write_mutex;
  std::atomic<bool>                              closed{false};
};

// nghttp2 callbacks ─────────────────────────────────────────────────────────

static int on_begin_headers_callback(nghttp2_session*, const nghttp2_frame* frame,
                                     void* user_data) {
  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_REQUEST)
    return 0;
  auto* ctx = static_cast<SessionContext*>(user_data);
  ctx->streams[frame->hd.stream_id] = StreamData{};
  return 0;
}

static int on_header_callback(nghttp2_session*, const nghttp2_frame* frame,
                               const uint8_t* name, size_t namelen,
                               const uint8_t* value, size_t valuelen,
                               uint8_t, void* user_data) {
  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_REQUEST)
    return 0;
  auto* ctx = static_cast<SessionContext*>(user_data);
  auto  it  = ctx->streams.find(frame->hd.stream_id);
  if (it == ctx->streams.end()) return 0;

  std::string n(reinterpret_cast<const char*>(name),  namelen);
  std::string v(reinterpret_cast<const char*>(value), valuelen);

  auto& req = it->second.request;
  if (n == ":path")   req.path   = v;
  if (n == ":method") req.method = v;
  if (n == "content-type") {
    req.content_type = v;
    req.is_streaming = buf_connect_server::connect::IsStreamingContentType(v);
  }
  if (n == "authorization") {
    req.bearer_token = buf_connect_server::connect::ExtractBearerToken(v);
  }
  req.headers[n] = v;
  return 0;
}

static int on_data_chunk_recv_callback(nghttp2_session*, uint8_t,
                                        int32_t stream_id,
                                        const uint8_t* data, size_t len,
                                        void* user_data) {
  auto* ctx = static_cast<SessionContext*>(user_data);
  auto  it  = ctx->streams.find(stream_id);
  if (it == ctx->streams.end()) return 0;
  it->second.body_buf.insert(it->second.body_buf.end(), data, data + len);
  return 0;
}

static int on_request_recv(nghttp2_session* session, SessionContext* ctx, int32_t stream_id)
{
    auto it = ctx->streams.find(stream_id);
    if (it == ctx->streams.end()) return 0;

    auto& sd = it->second;
    sd.request.body = sd.body_buf;

    // ── Shared state between header_fn and write_fn ──────────────────────────
    // header_fn just collects. write_fn submits everything at once.
    auto pending = std::make_shared<std::vector<std::pair<std::string,std::string>>>();
    auto response_submitted = std::make_shared<bool>(false);

    // ── header_fn: pure collector, never touches nghttp2 ────────────────────
    auto header_fn = [pending](const std::string& name, const std::string& value) {
        pending->emplace_back(name, value);
    };

    // ── write_fn: submits headers + body in one atomic nghttp2 operation ─────
    auto write_fn = [ctx, session, stream_id, pending, response_submitted](
            std::span<const uint8_t> data) -> bool {
        if (ctx->closed) return false;
        std::lock_guard<std::mutex> lock(ctx->write_mutex);

        if (!*response_submitted) {
            // Build NV array from everything header_fn collected
            std::vector<nghttp2_nv> nva;
            nva.reserve(pending->size());
            for (auto& [n, v] : *pending) {
                nva.push_back({
                                      reinterpret_cast<uint8_t*>(const_cast<char*>(n.data())),
                                      reinterpret_cast<uint8_t*>(const_cast<char*>(v.data())),
                                      n.size(), v.size(),
                                      NGHTTP2_NV_FLAG_NO_INDEX
                              });
            }

            if (data.empty()) {
                // Headers-only response (e.g. streaming: headers sent before loop)
                nghttp2_submit_response(session, stream_id,
                                        nva.data(), nva.size(), nullptr);
                *response_submitted = true;
                return !ctx->closed.load();
            }

            // Copy body — the read_callback must outlive this call
            struct BodyState {
                std::vector<uint8_t> data;
                size_t offset = 0;
            };
            auto* bs = new BodyState{{data.begin(), data.end()}, 0};

            nghttp2_data_provider provider{};
            provider.source.ptr = bs;
            provider.read_callback = [](nghttp2_session*, int32_t,
                                        uint8_t* buf, size_t length,
                                        uint32_t* flags,
                                        nghttp2_data_source* source,
                                        void*) -> ssize_t {
                auto* s = static_cast<BodyState*>(source->ptr);
                size_t remaining = s->data.size() - s->offset;
                size_t to_copy   = std::min(length, remaining);
                std::memcpy(buf, s->data.data() + s->offset, to_copy);
                s->offset += to_copy;
                if (s->offset >= s->data.size()) {
                    *flags |= NGHTTP2_DATA_FLAG_EOF;
                    delete s;
                }
                return static_cast<ssize_t>(to_copy);
            };

            // Single call: headers + data provider together
            nghttp2_submit_response(session, stream_id,
                                    nva.data(), nva.size(), &provider);
            *response_submitted = true;

        } else {
            // Subsequent write_fn calls = streaming frames after headers were sent
            // (WatchSessionEvents, StreamData)
            struct BodyState {
                nghttp2_data_provider provider{};
                std::vector<uint8_t> data;
                size_t offset = 0;
            };
            auto* bs = new BodyState{{}, {data.begin(), data.end()}, 0};

            bs->provider.source.ptr = bs;
            bs->provider.read_callback = [](nghttp2_session*, int32_t,
                                        uint8_t* buf, size_t length,
                                        uint32_t* flags,
                                        nghttp2_data_source* source,
                                        void*) -> ssize_t {
                auto* s = static_cast<BodyState*>(source->ptr);
                size_t remaining = s->data.size() - s->offset;
                size_t to_copy   = std::min(length, remaining);
                std::memcpy(buf, s->data.data() + s->offset, to_copy);
                s->offset += to_copy;
                if (s->offset >= s->data.size()) {
                    *flags |= NGHTTP2_DATA_FLAG_EOF;
                    delete s;
                }
                return static_cast<ssize_t>(to_copy);
            };

            nghttp2_submit_data(session, NGHTTP2_FLAG_NONE, stream_id, &bs->provider);
            nghttp2_session_send(session);
        }

        return !ctx->closed.load();
    };

    if (sd.request.is_streaming) {
        // Streaming handler: run on its own thread so ServeConnection
        // continues the recv loop and nghttp2_session_send keeps firing.
        std::thread([sd = std::move(sd), write_fn, header_fn, ctx_shared = ctx]() mutable {
            buf_connect_server::connect::ConnectResponseWriter writer(write_fn, header_fn, true);
            if (ctx_shared->handler) ctx_shared->handler(sd.request, writer);
        }).detach();
    } else {

        buf_connect_server::connect::ConnectResponseWriter writer(write_fn, header_fn, sd.request.is_streaming);

        if (ctx->handler) {
            ctx->handler(sd.request, writer);
        }
        else {
            writer.SendHeaders(buf_connect_server::connect::kHttpInternalError,
                               "application/json");
            writer.WriteError("internal", "no handler registered");
        }
    }
    ctx->streams.erase(it);
    // RST only on error — for clean streams nghttp2 closes via DATA+EOF flag
    return 0;
}

static int on_frame_recv_callback(nghttp2_session* session,
                                   const nghttp2_frame* frame,
                                   void* user_data)
{
    if (frame->hd.type == NGHTTP2_SETTINGS &&
        !(frame->hd.flags & NGHTTP2_FLAG_ACK)) {
        for (size_t i = 0; i < frame->settings.niv; ++i) {
            if (frame->settings.iv[i].settings_id == NGHTTP2_SETTINGS_HEADER_TABLE_SIZE &&
                frame->settings.iv[i].value == 0) {
                // NGINX asked us to use table size 0. Accept it (we must ACK)
                // but immediately re-negotiate back to 4096 so nghttp2's encoder
                // doesn't prepend the 0x20 shrink announcement on next HEADERS frame.
                nghttp2_settings_entry iv[] = {{NGHTTP2_SETTINGS_HEADER_TABLE_SIZE, 4096}};
                nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 1);
                break;
            }
        }
    }
  auto* ctx = static_cast<SessionContext*>(user_data);
  if ((frame->hd.flags & NGHTTP2_FLAG_END_STREAM) == 0) return 0;
  switch (frame->hd.type) {
    case NGHTTP2_DATA:
    case NGHTTP2_HEADERS:
      on_request_recv(session, ctx, frame->hd.stream_id);
      break;
    default:
      break;
  }
  return 0;
}

static ssize_t send_callback(nghttp2_session*, const uint8_t* data, size_t length,
                              int, void* user_data) {
  auto* ctx = static_cast<SessionContext*>(user_data);
  if (ctx->ssl) {
    int n = SSL_write(ctx->ssl, data, static_cast<int>(length));
    return n <= 0 ? NGHTTP2_ERR_CALLBACK_FAILURE : n;
  }
  ssize_t n = ::send(ctx->fd, data, length, MSG_NOSIGNAL);
  return n < 0 ? NGHTTP2_ERR_CALLBACK_FAILURE : n;
}

// Http2Listener::Impl ────────────────────────────────────────────────────────

class buf_connect_server::http2::Http2Listener::Impl {
 public:
  std::string     host_;
  uint16_t        port_;
  bool            tls_enabled_;
  std::string     cert_path_;
  std::string     key_path_;
  RequestHandler  handler_;
  std::atomic<bool> running_{false};
  int             server_fd_ = -1;
  SSL_CTX*        ssl_ctx_   = nullptr;
  std::thread     accept_thread_;

  void RunAcceptLoop() {
    while (running_) {
      sockaddr_in client_addr{};
      socklen_t   addr_len = sizeof(client_addr);
      int cfd = ::accept(server_fd_,
                         reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        char ip[addr_len];
        auto a = inet_ntop(AF_INET, &(client_addr.sin_addr), ip, addr_len);
        if (cfd < 0) {
        if (running_) spdlog::warn("Http2Listener: accept() failed");
        continue;
      }
      std::thread([this, cfd]() { ServeConnection(cfd); }).detach();
    }
  }

  void ServeConnection(int cfd) {
    auto ctx = std::make_shared<SessionContext>();
    ctx->fd  = cfd;

    if (tls_enabled_ && ssl_ctx_) {
      ctx->ssl = SSL_new(ssl_ctx_);
      SSL_set_fd(ctx->ssl, cfd);
      if (SSL_accept(ctx->ssl) <= 0) {
        SSL_free(ctx->ssl);
        ::close(cfd);
        return;
      }
    }

    ctx->handler = handler_;

    nghttp2_session_callbacks* cbs;
    nghttp2_session_callbacks_new(&cbs);
    nghttp2_session_callbacks_set_send_callback(cbs, send_callback);
    nghttp2_session_callbacks_set_on_begin_headers_callback(
        cbs, on_begin_headers_callback);
    nghttp2_session_callbacks_set_on_header_callback(cbs, on_header_callback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
        cbs, on_data_chunk_recv_callback);
    nghttp2_session_callbacks_set_on_frame_recv_callback(
        cbs, on_frame_recv_callback);

    nghttp2_session_server_new(&ctx->session, cbs, ctx.get());
    nghttp2_session_callbacks_del(cbs);

    // Send server connection preface
    nghttp2_settings_entry iv[] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
        {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, 65535}};
    nghttp2_submit_settings(ctx->session, NGHTTP2_FLAG_NONE, iv, 2);
    nghttp2_session_send(ctx->session);

    std::vector<uint8_t> read_buf(65536);
    while (!ctx->closed) {
      ssize_t nread;
      if (ctx->ssl) {
        nread = SSL_read(ctx->ssl, read_buf.data(),
                         static_cast<int>(read_buf.size()));
      } else {
        nread = ::recv(cfd, read_buf.data(), read_buf.size(), 0);
      }
      if (nread <= 0) { ctx->closed = true; break; }
      int rv = nghttp2_session_mem_recv(
          ctx->session, read_buf.data(),
          static_cast<size_t>(nread));
      if (rv < 0) { ctx->closed = true; break; }
      nghttp2_session_send(ctx->session);
    }

    nghttp2_session_del(ctx->session);
    if (ctx->ssl) { SSL_shutdown(ctx->ssl); SSL_free(ctx->ssl); }
    ::close(cfd);
  }
};

buf_connect_server::http2::Http2Listener::Http2Listener(
    const std::string& host, uint16_t port,
    bool tls_enabled,
    const std::string& cert_path,
    const std::string& key_path)
    : impl_(std::make_unique<Impl>()) {
  impl_->host_        = host;
  impl_->port_        = port;
  impl_->tls_enabled_ = tls_enabled;
  impl_->cert_path_   = cert_path;
  impl_->key_path_    = key_path;
}

buf_connect_server::http2::Http2Listener::~Http2Listener() { Stop(); }

void buf_connect_server::http2::Http2Listener::SetRequestHandler(
    RequestHandler handler) {
  impl_->handler_ = std::move(handler);
}

void buf_connect_server::http2::Http2Listener::Start() {
  if (impl_->tls_enabled_) {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    impl_->ssl_ctx_ = SSL_CTX_new(TLS_server_method());
    SSL_CTX_use_certificate_file(impl_->ssl_ctx_, impl_->cert_path_.c_str(),
                                  SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(impl_->ssl_ctx_, impl_->key_path_.c_str(),
                                 SSL_FILETYPE_PEM);
  }

  impl_->server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  ::setsockopt(impl_->server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(impl_->port_);
  ::inet_pton(AF_INET, impl_->host_.c_str(), &addr.sin_addr);
  ::bind(impl_->server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  ::listen(impl_->server_fd_, 128);

  impl_->running_ = true;
  impl_->accept_thread_ = std::thread([this]() { impl_->RunAcceptLoop(); });
  spdlog::info("Http2Listener bound to {}:{}", impl_->host_, impl_->port_);
}

void buf_connect_server::http2::Http2Listener::Stop() {
  impl_->running_ = false;
  if (impl_->server_fd_ >= 0) {
    ::shutdown(impl_->server_fd_, SHUT_RDWR);
    ::close(impl_->server_fd_);
    impl_->server_fd_ = -1;
  }
  if (impl_->accept_thread_.joinable()) impl_->accept_thread_.join();
  if (impl_->ssl_ctx_) { SSL_CTX_free(impl_->ssl_ctx_); impl_->ssl_ctx_ = nullptr; }
}
