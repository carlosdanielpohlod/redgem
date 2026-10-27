// What arrives from another machine: frames cut from a stream of bytes that arrives in pieces,
// the MessagePack in them read within a message's limits, and the message checked before anything
// uses it.
#include <algorithm>
#include <cstdint>
#include <string>

#include "fuzz/fuzz_target.hpp"
#include "net/frame_buffer.hpp"
#include "net/wire.hpp"
#include "net/wire_codec.hpp"

namespace {

// The first byte of an input is not part of what arrives: it says how big the pieces it arrives
// in are (1 to 256 bytes).
constexpr std::size_t kPieceSizes = 256;

void check_message(const std::string& bytes) {
  const auto message = redgem::net::wire::unpacked(bytes);
  if (!message || !redgem::net::wire::well_formed(*message)) return;
  redgem::net::wire::is_game_message(*message);
  redgem::net::wire::protocol_of(*message);
  redgem::net::wire::log_text(*message);
}

// Every message the frames that arrived so far hold, read as a connection reads them.
void read_waiting(redgem::net::FrameBuffer& frames) {
  while (frames.message_waiting() && !frames.broken()) {
    const auto bytes = frames.next();
    if (bytes) check_message(*bytes);
  }
}

void read_frames(const std::uint8_t* data, std::size_t size, std::size_t piece) {
  redgem::net::FrameBuffer frames;
  for (std::size_t at = 0; at < size && !frames.broken(); at += piece) {
    frames.append(data + at, std::min(piece, size - at));
    read_waiting(frames);
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size == 0) return redgem::fuzz::kKeepInput;
  // What arrives as one message's bytes, and as a stream of frames.
  check_message(std::string(reinterpret_cast<const char*>(data + 1), size - 1));
  read_frames(data + 1, size - 1, 1 + data[0] % kPieceSizes);
  return redgem::fuzz::kKeepInput;
}
