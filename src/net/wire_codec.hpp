#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// How a message travels: MessagePack (https://msgpack.org), framed (frame_buffer.hpp). A message
// is a map whose keys are the one-letter ones of wire.hpp; a game's data is MessagePack's own nil,
// booleans, integers, 64-bit floats, strings (UTF-8), arrays and maps. What arrives is untrusted:
// it is read with the limits of what a message may be checked as it is read (how deep it nests, how many things an
// array or a map holds, text that is UTF-8), so that nothing past them is ever built in memory, and checked again
// (wire::well_formed) before any of it is used. With REDGEM_LOG_MESSAGES=1 in its environment, a program writes every
// message it sends and receives to its log, as JSON text.
namespace redgem::net::wire {

// A message in MessagePack, with text that is not valid UTF-8 replaced instead of refused; and
// as the bytes that travel on a connection, framed.
std::vector<std::uint8_t> msgpack(const nlohmann::json& message);
std::vector<std::uint8_t> packed(const nlohmann::json& message);
// The message in the bytes of a frame; nothing when they are not one within the limits.
std::optional<nlohmann::json> unpacked(const std::string& bytes);
// Whether `text` is valid UTF-8, which every text of a message is.
bool valid_utf8(const std::string& text);

}  // namespace redgem::net::wire
