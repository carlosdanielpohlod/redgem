#include <doctest/doctest.h>

#include "http_compression.hpp"
#include "net/http/http_encoding.hpp"
#include "net/http/http_message.hpp"

using namespace redgem::net;
using redgem::test::compressed;
using redgem::test::kGzipWrapper;
using redgem::test::kNoWrapper;
using redgem::test::kZlibWrapper;

namespace {

constexpr int kOk = 200;
constexpr std::size_t kTextLetters = 1000;
const std::string kText = "{\"news\":\"" + std::string(kTextLetters, 'n') + "\"}";
constexpr std::size_t kGzipTrailerBytes = 8;  // the text's CRC-32, then its size
constexpr char kFlippedBits = 0x55;
// A raw deflate block of the type the format keeps reserved (the low bits 111: last, type 3).
const std::string kReservedDeflateBlock = "\x07 is no deflate";

HttpResponse answer(const std::string& encoding, const std::string& body) {
  HttpResponse response;
  response.status = kOk;
  response.headers = {{"content-type", "application/json"},
                      {"content-encoding", encoding},
                      {"content-length", std::to_string(body.size())}};
  response.body = body;
  return response;
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("a gzip answer comes decompressed, without the headers that described its bytes") {
  for (const char* gzip : {"gzip", "x-gzip", " GZIP "}) {
    HttpResponse response = answer(gzip, compressed(kText, kGzipWrapper));
    CHECK_MESSAGE(decode_body(response).empty(), gzip);
    CHECK(response.body == kText);
    CHECK(header_value(response.headers, "content-encoding").empty());
    CHECK(header_value(response.headers, "content-length").empty());
    CHECK(header_value(response.headers, "content-type") == "application/json");
  }
}

TEST_CASE("a deflate answer comes decompressed, with zlib's wrapper or without") {
  for (const int window : {kZlibWrapper, kNoWrapper}) {
    HttpResponse response = answer("deflate", compressed(kText, window));
    CHECK_MESSAGE(decode_body(response).empty(), window);
    CHECK(response.body == kText);
  }
}

TEST_CASE("an answer with no encoding the engine reads stays as it came") {
  for (const char* encoding : {"", "identity", "br", "gzip, br"}) {
    HttpResponse response = answer(encoding, "as it came");
    CHECK(decode_body(response).empty());
    CHECK(response.body == "as it came");
    CHECK(header_value(response.headers, "content-encoding") == encoding);
  }
  HttpResponse empty = answer("gzip", "");
  CHECK(decode_body(empty).empty());
  CHECK(empty.body.empty());
}

TEST_CASE("an answer that would grow past the limit once decompressed is refused") {
  HttpResponse fits = answer("gzip", compressed(std::string(kMaxHttpDecodedBodyBytes, '0'), kGzipWrapper));
  CHECK(decode_body(fits).empty());
  CHECK(fits.body.size() == kMaxHttpDecodedBodyBytes);

  const std::string bomb = compressed(std::string(kMaxHttpDecodedBodyBytes + 1, '0'), kGzipWrapper);
  CHECK(bomb.size() < kMaxHttpDecodedBodyBytes / 100);  // (a few kilobytes on the wire)
  HttpResponse refused = answer("gzip", bomb);
  CHECK(contains(decode_body(refused), "once decompressed"));
  CHECK(refused.body == bomb);
}

TEST_CASE("a corrupt or cut compressed body is refused, saying so") {
  // (The checksum of the text is the first of gzip's trailer: the text no longer matches it.)
  std::string damaged = compressed(kText, kGzipWrapper);
  damaged[damaged.size() - kGzipTrailerBytes] ^= kFlippedBits;
  HttpResponse corrupt = answer("gzip", damaged);
  CHECK(contains(decode_body(corrupt), "gzip body is corrupt"));

  HttpResponse not_gzip = answer("gzip", "this was never compressed");
  CHECK(contains(decode_body(not_gzip), "gzip body is corrupt"));
  HttpResponse not_deflate = answer("deflate", kReservedDeflateBlock);
  CHECK(contains(decode_body(not_deflate), "deflate body is corrupt"));

  const std::string whole = compressed(kText, kGzipWrapper);
  HttpResponse cut = answer("gzip", whole.substr(0, whole.size() / 2));
  CHECK(contains(decode_body(cut), "cut short"));

  HttpResponse trailing = answer("gzip", whole + "more");
  CHECK(contains(decode_body(trailing), "corrupt"));
}
