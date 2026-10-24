#include <doctest/doctest.h>

#include "net/http/http_response_parser.hpp"

using namespace redgem::net;

namespace {

constexpr int kOk = 200;
constexpr int kNoContent = 204;
constexpr int kNotFound = 404;

using State = HttpResponseParser::State;

void feed(HttpResponseParser& parser, const std::string& bytes) { parser.feed(bytes.data(), bytes.size()); }

// The answer read from `bytes` arriving a byte at a time: however the network cuts it up.
HttpResponseParser byte_by_byte(const std::string& bytes) {
  HttpResponseParser parser;
  for (const char byte : bytes) parser.feed(&byte, 1);
  return parser;
}

}  // namespace

TEST_CASE("an answer with a length is read whole, however it arrives") {
  const std::string answer =
      "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 11\r\n"
      "X-Spaced:   padded value  \r\n\r\n{\"top\":[1]}";
  HttpResponseParser parser = byte_by_byte(answer);
  REQUIRE(parser.state() == State::Done);
  const HttpResponse response = parser.take();
  CHECK(response.status == kOk);
  CHECK(response.body == "{\"top\":[1]}");
  CHECK(response.error.empty());
  CHECK(response.headers.front() == std::make_pair(std::string("content-type"), std::string("application/json")));
  CHECK(response.headers.back().second == "padded value");
}

TEST_CASE("an answer in chunks is put back together, extensions and trailers aside") {
  HttpResponseParser parser = byte_by_byte(
      "HTTP/1.1 404 Not Found\r\nTransfer-Encoding: chunked\r\n\r\n5;ext=1\r\nhello\r\n7\r\n, world\r\n0\r\n"
      "X-Trailer: yes\r\n\r\n");
  REQUIRE(parser.state() == State::Done);
  const HttpResponse response = parser.take();
  CHECK(response.status == kNotFound);
  CHECK(response.body == "hello, world");
}

TEST_CASE("an answer with no length lasts until the connection ends") {
  HttpResponseParser parser;
  feed(parser, "HTTP/1.0 200 OK\r\n\r\nall of ");
  feed(parser, "it");
  CHECK(parser.state() == State::Reading);
  parser.end_of_stream();
  REQUIRE(parser.state() == State::Done);
  CHECK(parser.take().body == "all of it");
}

TEST_CASE("interim answers are passed over and some answers have no body") {
  HttpResponseParser parser;
  feed(parser, "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 204 No Content\r\n\r\n");
  REQUIRE(parser.state() == State::Done);
  CHECK(parser.take().status == kNoContent);
}

TEST_CASE("an answer cut short, or that is not HTTP, fails with the reason") {
  HttpResponseParser cut;
  feed(cut, "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort");
  cut.end_of_stream();
  CHECK(cut.state() == State::Failed);
  CHECK(cut.take().error.find("ended before") != std::string::npos);

  for (const char* nonsense : {"SSH-2.0-OpenSSH\r\n\r\n", "HTTP/1.1 2x0 OK\r\n\r\n",
                               "HTTP/1.1 200 OK\r\nno colon\r\n\r\n", "HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n",
                               "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n"}) {
    HttpResponseParser parser;
    feed(parser, nonsense);
    CHECK_MESSAGE(parser.state() == State::Failed, nonsense);
    CHECK_FALSE(parser.take().error.empty());
  }
}

TEST_CASE("answers larger than a game may take are refused, early when they say so") {
  HttpResponseParser announced;
  feed(announced, "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(kMaxHttpBodyBytes + 1) + "\r\n\r\n");
  CHECK(announced.state() == State::Failed);
  CHECK(announced.take().error.find("larger than") != std::string::npos);

  HttpResponseParser endless;
  feed(endless, "HTTP/1.1 200 OK\r\n\r\n");
  const std::string piece(kMaxHttpBodyBytes / 4, 'a');
  for (int i = 0; i < 5 && endless.state() == State::Reading; ++i) feed(endless, piece);
  CHECK(endless.state() == State::Failed);

  HttpResponseParser huge_chunk;
  feed(huge_chunk, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFFFF\r\n");
  CHECK(huge_chunk.state() == State::Failed);

  HttpResponseParser endless_head;
  feed(endless_head, "HTTP/1.1 200 OK\r\n" + std::string(kMaxHttpHeadBytes + 1, 'a'));
  CHECK(endless_head.state() == State::Failed);
}

TEST_CASE("a connection cut without a goodbye leaves only an answer that said it was whole") {
  HttpResponseParser until_the_end;
  feed(until_the_end, "HTTP/1.1 200 OK\r\n\r\nall of it");
  until_the_end.cut_off();
  CHECK(until_the_end.state() == State::Failed);

  HttpResponseParser measured;
  feed(measured, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
  measured.cut_off();
  CHECK(measured.state() == State::Done);
  CHECK(measured.take().body == "ok");
}
