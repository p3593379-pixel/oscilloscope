#include "buf_connect_server/connect/response_writer.hpp"
#include "buf_connect_server/connect/frame_codec.hpp"
#include <nlohmann/json.hpp>
#include <string>

buf_connect_server::connect::ConnectResponseWriter::ConnectResponseWriter(
        WriteCallback write_fn, HeaderCallback header_fn, bool is_streaming)
        : write_fn_(std::move(write_fn)),
          header_fn_(std::move(header_fn)),
          is_streaming_(is_streaming) {}

void buf_connect_server::connect::ConnectResponseWriter::AddHeader(const std::string& name, const std::string& value)
{
    if (headers_sent_)
        return;
    pending_headers_.emplace_back(name, value);
}

void buf_connect_server::connect::ConnectResponseWriter::SendHeaders(int status_code, const std::string& content_type)
{
    if (headers_sent_)
        return;
    header_fn_(":status", std::to_string(status_code));
    header_fn_("content-type", content_type);
    for (const auto& [name, value] : pending_headers_) {
        header_fn_(name, value);
    }
    pending_headers_.clear();
    headers_sent_ = true;
}

void buf_connect_server::connect::ConnectResponseWriter::WriteUnary(std::span<const uint8_t> serialized_proto)
{
    if (!connected_)
        return;
    connected_ = write_fn_(serialized_proto);
}

void buf_connect_server::connect::ConnectResponseWriter::SendUnaryResponse(
        int status,
        std::string_view content_type,
        std::span<const uint8_t> payload)
{
    std::string len_str = std::to_string(payload.size());

    // :status MUST be first — HTTP/2 §8.3.1 requires all pseudo-headers
    // to precede regular headers in the HEADERS frame.
    header_fn_(":status",        std::to_string(status));
    header_fn_("content-type",   std::string(content_type));
    header_fn_("content-length", len_str);

    // Flush any extra headers queued via AddHeader() (e.g. set-cookie) AFTER
    for (const auto& [name, value] : pending_headers_) {
        header_fn_(name, value);
    }
    pending_headers_.clear();
    headers_sent_ = true;
    connected_ = write_fn_(payload);
}

void buf_connect_server::connect::ConnectResponseWriter::WriteStreamingFrame(std::span<const uint8_t> serialized_proto)
{
    if (!connected_)
        return;
    auto frame = EncodeFrame(serialized_proto, FrameFlag::kData);
    connected_ = write_fn_(std::span<const uint8_t>(frame));
}

void buf_connect_server::connect::ConnectResponseWriter::WriteEndOfStream() {
    if (!connected_) return;
    std::string json = "{\"metadata\":{}}";
    std::span<const uint8_t> payload(
            reinterpret_cast<const uint8_t*>(json.data()), json.size());
    auto frame = EncodeFrame(payload, FrameFlag::kEndStream);
    write_fn_(std::span<const uint8_t>(frame));
}

void buf_connect_server::connect::ConnectResponseWriter::WriteError(
        const std::string& code, const std::string& message) {
    nlohmann::json err;
    err["code"]    = code;
    err["message"] = message;
    std::string body = err.dump();
    std::span<const uint8_t> payload(
            reinterpret_cast<const uint8_t*>(body.data()), body.size());
    write_fn_(payload);
}

bool buf_connect_server::connect::ConnectResponseWriter::IsClientConnected() const {
    return connected_;
}

void buf_connect_server::connect::ConnectResponseWriter::SetDisconnected() {
    connected_ = false;
}
