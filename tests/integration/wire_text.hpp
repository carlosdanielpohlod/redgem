#pragma once

// Messages written in tests as JSON text, which reads easily, turned into the bytes that travel
// (MessagePack, framed), and back.

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "net/wire.hpp"
#include "net/frame_buffer.hpp"
#include "net/wire_codec.hpp"

namespace redgem::test {

// Whether a message is of a kind that says the version of the protocol: a hello or a welcome.
inline bool says_version(const nlohmann::json& message) {
  const nlohmann::json kind =
      message.is_object() ? message.value(net::wire::kKind, nlohmann::json()) : nlohmann::json();
  return kind == net::wire::kHello || kind == net::wire::kWelcome;
}

// The bytes of the message `text` says. A hello or a welcome speaks this program's version of the
// protocol unless it says another ("v").
inline std::vector<std::uint8_t> wire_frame(const std::string& text) {
  nlohmann::json message = nlohmann::json::parse(text);
  if (says_version(message) && !message.contains(net::wire::kVersion)) {
    message[net::wire::kVersion] = net::wire::kProtocolVersion;
  }
  return net::wire::packed(message);
}

// Any bytes, framed as a message is (for what is not one).
inline std::vector<std::uint8_t> raw_frame(const std::string& bytes) {
  return net::frame(std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
}

// The message in the bytes of a frame (null when they are not one).
inline nlohmann::json message_in(const std::string& bytes) {
  return net::wire::unpacked(bytes).value_or(nlohmann::json());
}

}  // namespace redgem::test
