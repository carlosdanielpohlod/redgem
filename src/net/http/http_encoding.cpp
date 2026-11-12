// Answers sent compressed (Content-Encoding: gzip or deflate), decompressed with zlib.

#include "net/http/http_encoding.hpp"

#include <zlib.h>

#include "net/http/http_message.hpp"

namespace redgem::net {

namespace {

constexpr const char* kContentEncoding = "content-encoding";
constexpr const char* kContentLength = "content-length";
constexpr const char* kGzip = "gzip";
constexpr const char* kLegacyGzip = "x-gzip";
constexpr const char* kDeflate = "deflate";
// zlib's window sizes say which wrapper the data has: 16 more for gzip's, negative for none.
constexpr int kGzipWrapperFlag = 16;
constexpr int kGzipWindow = MAX_WBITS + kGzipWrapperFlag;
constexpr int kZlibWindow = MAX_WBITS;
constexpr int kRawWindow = -MAX_WBITS;
// How much is decompressed at a time: the body grows by at most this past the limit before it is refused.
constexpr std::size_t kInflateStepBytes = 64 * 1024;
// zlib's wrapper starts with two bytes: deflate as the method in the low bits of the first, and
// the two of them read as one number a multiple of 31.
constexpr std::size_t kZlibHeaderBytes = 2;
constexpr unsigned kZlibMethodMask = 0x0F;
constexpr unsigned kZlibHeaderCheck = 31;
constexpr unsigned kBitsPerByte = 8;

enum class Coding { None, Gzip, Deflate };
// How decompressing went, so far or in the end.
enum class Inflated { Going, Whole, TooLarge, CutShort, Corrupt };

bool is_gzip(const std::string& name) { return name == kGzip || name == kLegacyGzip; }

Coding coding_of(const std::string& name) {
  if (is_gzip(name)) return Coding::Gzip;
  if (name == kDeflate) return Coding::Deflate;
  return Coding::None;
}

bool nothing_to_decode(Coding coding, const std::string& body) { return coding == Coding::None || body.empty(); }

unsigned byte_at(const std::string& text, std::size_t at) { return static_cast<unsigned char>(text[at]); }

// Whether deflate data starts with zlib's wrapper (as the standard says it does) rather than raw.
bool has_zlib_header(const std::string& body) {
  if (body.size() < kZlibHeaderBytes) return false;
  const unsigned first = byte_at(body, 0);
  const bool deflate_method = (first & kZlibMethodMask) == Z_DEFLATED;
  return deflate_method && ((first << kBitsPerByte) | byte_at(body, 1)) % kZlibHeaderCheck == 0;
}

int window_for(Coding coding, const std::string& body) {
  if (coding == Coding::Gzip) return kGzipWindow;
  return has_zlib_header(body) ? kZlibWindow : kRawWindow;
}

Inflated outcome(int result, std::size_t size) {
  if (size > kMaxHttpDecodedBodyBytes) return Inflated::TooLarge;
  if (result == Z_OK) return Inflated::Going;
  if (result == Z_STREAM_END) return Inflated::Whole;
  if (result == Z_BUF_ERROR) return Inflated::CutShort;  // (all the input read, and the stream not ended)
  return Inflated::Corrupt;
}

// Decompresses what fits in one more step's room at the end of `out`.
Inflated inflate_step(z_stream& stream, std::string& out) {
  const std::size_t before = out.size();
  out.resize(before + kInflateStepBytes);
  stream.next_out = reinterpret_cast<Bytef*>(&out[before]);
  stream.avail_out = static_cast<uInt>(kInflateStepBytes);
  const int result = inflate(&stream, Z_NO_FLUSH);
  out.resize(before + kInflateStepBytes - stream.avail_out);
  return outcome(result, out.size());
}

// Bytes after the compressed stream's end are no part of it: the body is not what it says.
Inflated checked_end(Inflated state, const z_stream& stream) {
  if (state != Inflated::Whole) return state;
  return stream.avail_in == 0 ? Inflated::Whole : Inflated::Corrupt;
}

Inflated inflate_all(const std::string& compressed, int window, std::string& out) {
  z_stream stream{};
  if (inflateInit2(&stream, window) != Z_OK) return Inflated::Corrupt;
  // (An answer's body is at most kMaxHttpBodyBytes: its size fits zlib's counts.)
  stream.next_in = reinterpret_cast<const Bytef*>(compressed.data());
  stream.avail_in = static_cast<uInt>(compressed.size());
  Inflated state = Inflated::Going;
  while (state == Inflated::Going) state = inflate_step(stream, out);
  state = checked_end(state, stream);
  inflateEnd(&stream);
  return state;
}

std::string problem(Inflated state, const std::string& coding) {
  switch (state) {
    case Inflated::Whole:
      return {};
    case Inflated::TooLarge:
      return "the answer is larger than " + std::to_string(kMaxHttpDecodedBodyBytes) + " bytes once decompressed";
    case Inflated::CutShort:
      return "the answer's " + coding + " body is cut short";
    default:
      return "the answer's " + coding + " body is corrupt";
  }
}

}  // namespace

std::string decode_body(HttpResponse& response) {
  const std::string coding = lower_case(trimmed(header_value(response.headers, kContentEncoding)));
  const Coding kind = coding_of(coding);
  if (nothing_to_decode(kind, response.body)) return {};
  std::string decoded;
  std::string why = problem(inflate_all(response.body, window_for(kind, response.body), decoded), coding);
  if (!why.empty()) return why;
  response.body = std::move(decoded);
  remove_header(response.headers, kContentEncoding);
  remove_header(response.headers, kContentLength);
  return {};
}

}  // namespace redgem::net
