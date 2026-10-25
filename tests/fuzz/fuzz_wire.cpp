// What arrives from another machine: frames cut from a stream of bytes, the MessagePack in them
// read within a message's limits, and the message checked before anything uses it.
#include <cstdint>
#include <string>

#include "fuzz/fuzz_target.hpp"
#include "net/frame_buffer.hpp"
#include "net/wire.hpp"
#include "net/wire_codec.hpp"

namespace {

void check_message(const std::string& bytes) {
  const auto message = redgem::net::wire::unpacked(bytes);
  if (!message || !redgem::net::wire::well_formed(*message)) return;
  redgem::net::wire::is_game_message(*message);
  redgem::net::wire::protocol_of(*message);
  redgem::net::wire::log_text(*message);
}

// The input as a stream: framed messages, one after the other, read as a connection reads them.
void read_frames(const std::uint8_t* data, std::size_t size) {
  redgem::net::FrameBuffer frames;
  frames.append(data, size);
  while (frames.message_waiting() && !frames.broken()) {
    const auto bytes = frames.next();
    if (bytes) check_message(*bytes);
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  // The input as one message's bytes, and as a stream of frames.
  check_message(std::string(reinterpret_cast<const char*>(data), size));
  read_frames(data, size);
  return redgem::fuzz::kKeepInput;
}
