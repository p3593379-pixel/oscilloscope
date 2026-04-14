#ifndef BUF_CONNECT_SERVER_CONNECT_RESPONSE_WRITER_HPP
#define BUF_CONNECT_SERVER_CONNECT_RESPONSE_WRITER_HPP

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace buf_connect_server::connect {

// Callback types for writing to the underlying HTTP/2 stream
    using WriteCallback  = std::function<bool(std::span<const uint8_t>)>;
    using HeaderCallback = std::function<void(const std::string&, const std::string&)>;

    class ConnectResponseWriter {
    public:
        ConnectResponseWriter(WriteCallback  write_fn,
                              HeaderCallback header_fn,
                              bool           is_streaming);

        // Inject an extra response header (e.g. Set-Cookie).
        // Must be called BEFORE SendHeaders().
        void AddHeader(const std::string& name, const std::string& value);

        // Send HTTP response headers
        // Flushes any headers queued via AddHeader().
        void SendHeaders(int status_code, const std::string& content_type);

        // Sends raw protobuf bytes without length prefix.
        void WriteUnary(std::span<const uint8_t> serialized_proto);

        // Send whole response
        // DO use it for unary responses
        void SendUnaryResponse(int status, std::string_view content_type, std::span<const uint8_t> payload);

        // Write a single streaming frame with 5-byte length prefix.
        void WriteStreamingFrame(std::span<const uint8_t> serialized_proto);

        // Send end-of-stream frame (flag=0x02, empty payload JSON {})
        void WriteEndOfStream();

        // Write a Connect error response as JSON body.
        void WriteError(const std::string& code, const std::string& message);

        // Returns false when the client has disconnected.
        bool IsClientConnected() const;

        void SetDisconnected();

    private:
        WriteCallback  write_fn_;
        HeaderCallback header_fn_;
        bool           is_streaming_;
        bool           headers_sent_  = false;
        bool           connected_     = true;

        // Extra headers accumulated before SendHeaders() is called
        std::vector<std::pair<std::string, std::string>> pending_headers_;
    };

}  // namespace buf_connect_server::connect

#endif  // BUF_CONNECT_SERVER_CONNECT_RESPONSE_WRITER_HPP
