// Parsing a web server's answer: the status line, headers and a chunked JSON body, fed in the
// pieces a network delivers (one TCP segment at a time), as the HTTP client reads every answer.

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>

#include "bench/bench_case.hpp"
#include "net/http/http_response_parser.hpp"

namespace redgem::bench {

namespace {

constexpr int kChunks = 16;
constexpr std::size_t kChunkBytes = 4096;
constexpr std::size_t kSegmentBytes = 1460;  // what one TCP segment carries on most networks
constexpr int kParsesPerSample = 200;
constexpr int kStatusOk = 200;
constexpr std::size_t kHexBytes = 16;

std::string chunk(std::size_t bytes, char fill) {
  char size[kHexBytes];
  std::snprintf(size, sizeof(size), "%zx", bytes);
  return std::string(size) + "\r\n" + std::string(bytes, fill) + "\r\n";
}

std::string answer() {
  std::string text =
      "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nCache-Control: no-cache\r\n"
      "Server: bench\r\nTransfer-Encoding: chunked\r\n\r\n";
  for (int i = 0; i < kChunks; ++i) text += chunk(kChunkBytes, static_cast<char>('a' + i));
  return text + "0\r\n\r\n";
}

void parse(const std::string& text) {
  net::HttpResponseParser parser;
  for (std::size_t at = 0; at < text.size(); at += kSegmentBytes)
    parser.feed(text.data() + at, std::min(kSegmentBytes, text.size() - at));
  require(parser.state() == net::HttpResponseParser::State::Done, "parsing the answer");
  const net::HttpResponse response = parser.take();
  require(response.status == kStatusOk, "reading the answer's status");
  keep(response.body.size());
}

}  // namespace

BenchCase http_case() {
  const std::string what =
      "parsing an answer with a chunked body of " + std::to_string(kChunks * kChunkBytes) + " bytes";
  return {"http.parse_response", what, [] {
            auto text = std::make_shared<std::string>(answer());
            return Sampler([text] { return microseconds_each(kParsesPerSample, [&text] { parse(*text); }); });
          }};
}

}  // namespace redgem::bench
