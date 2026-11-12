#pragma once

// Compressing what the tests of HTTP serve, as a server would before sending it (gzip, deflate).

#include <zlib.h>

#include <stdexcept>
#include <string>

namespace redgem::test {

// zlib's window sizes, which say the wrapper: gzip's, zlib's (deflate as the standard has it) or
// none (raw deflate, as some servers send it).
inline constexpr int kGzipWrapper = MAX_WBITS + 16;
inline constexpr int kZlibWrapper = MAX_WBITS;
inline constexpr int kNoWrapper = -MAX_WBITS;
inline constexpr int kMemoryLevel = 8;  // zlib's default

// `text` compressed with deflate, in the wrapper `window` says.
inline std::string compressed(const std::string& text, int window) {
  z_stream stream{};
  if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, window, kMemoryLevel, Z_DEFAULT_STRATEGY) != Z_OK) {
    throw std::runtime_error("cannot start compressing");
  }
  std::string out(deflateBound(&stream, static_cast<uLong>(text.size())), '\0');
  stream.next_in = reinterpret_cast<const Bytef*>(text.data());
  stream.avail_in = static_cast<uInt>(text.size());
  stream.next_out = reinterpret_cast<Bytef*>(&out[0]);
  stream.avail_out = static_cast<uInt>(out.size());
  const int result = deflate(&stream, Z_FINISH);
  out.resize(stream.total_out);
  deflateEnd(&stream);
  if (result != Z_STREAM_END) throw std::runtime_error("cannot compress");
  return out;
}

}  // namespace redgem::test
