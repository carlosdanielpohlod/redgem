#pragma once

// Messages written in tests as JSON text, which reads easily, turned into the bytes that travel
// (MessagePack, framed), and back.

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "net/wire_codec.hpp"

namespace redgem::test {

// The bytes of the message `text` says.
inline std::vector<std::uint8_t> wire_frame(const std::string& text) {
  return net::wire::packed(nlohmann::json::parse(text));
}

// The message in the bytes of a frame (null when they are not one).
inline nlohmann::json message_in(const std::string& bytes) {
  return net::wire::unpacked(bytes).value_or(nlohmann::json());
}

}  // namespace redgem::test
