// An answer to a web request as it arrives from a server: the status line, headers and a body
// sized by its length, in chunks or up to the end of the connection, fed in pieces.
#include <algorithm>
#include <cstdint>

#include "fuzz/fuzz_target.hpp"
#include "net/http/http_response_parser.hpp"

namespace {

using redgem::net::HttpResponseParser;

// The first byte of an input is not part of the answer: its low bits say how big the pieces it
// arrives in are (1 to 128 bytes), its high bit how the connection ends.
constexpr std::uint8_t kPieceBits = 0x7F;
constexpr std::uint8_t kVouchedEndBit = 0x80;

void feed_in_pieces(HttpResponseParser& parser, const char* bytes, std::size_t size, std::size_t piece) {
  for (std::size_t at = 0; at < size && parser.state() == HttpResponseParser::State::Reading; at += piece) {
    parser.feed(bytes + at, std::min(piece, size - at));
  }
}

// The connection ends as TLS ends it with close_notify (or plain TCP does), or cut off.
void end_connection(HttpResponseParser& parser, std::uint8_t control) {
  if ((control & kVouchedEndBit) != 0) {
    parser.end_of_stream();
  } else {
    parser.cut_off();
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size == 0) return redgem::fuzz::kKeepInput;
  const std::uint8_t control = data[0];
  HttpResponseParser parser;
  feed_in_pieces(parser, reinterpret_cast<const char*>(data + 1), size - 1, 1 + (control & kPieceBits));
  end_connection(parser, control);
  parser.take();
  return redgem::fuzz::kKeepInput;
}
