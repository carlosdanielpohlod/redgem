#include <doctest/doctest.h>

#include "net/http/http_message.hpp"

using namespace redgem::net;

namespace {

Url url_of(const std::string& text) {
  std::string error;
  const auto url = parse_url(text, error);
  REQUIRE_MESSAGE(url.has_value(), error);
  return *url;
}

HttpRequest request(const std::string& method, HttpHeaders headers = {}, std::string body = {}) {
  HttpRequest made;
  made.method = method;
  made.url = "https://example.com/";
  made.headers = std::move(headers);
  made.body = std::move(body);
  return made;
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("a request goes out as HTTP/1.1 with its machine, its headers and its body") {
  const std::string text = request_text(request("POST", {{"Authorization", "Bearer abc"}}, "{\"points\":120}"),
                                        url_of("https://api.example.com:8443/scores?x=1"));
  CHECK(text.rfind("POST /scores?x=1 HTTP/1.1\r\n", 0) == 0);
  CHECK(contains(text, "\r\nHost: api.example.com:8443\r\n"));
  CHECK(contains(text, "\r\nConnection: close\r\n"));
  CHECK(contains(text, "\r\nUser-Agent: RedGem\r\n"));
  CHECK(contains(text, "\r\nContent-Length: 14\r\n"));
  CHECK(contains(text, "\r\nAuthorization: Bearer abc\r\n"));
  CHECK(text.substr(text.size() - std::string("\r\n\r\n{\"points\":120}").size()) == "\r\n\r\n{\"points\":120}");
}

TEST_CASE("a GET says nothing of a body, an empty POST says it has none, and a game may name itself") {
  const std::string get = request_text(request("GET", {{"user-agent", "MyGame/2"}}), url_of("https://example.com"));
  CHECK_FALSE(contains(get, "Content-Length"));
  CHECK_FALSE(contains(get, "RedGem"));
  CHECK(contains(get, "user-agent: MyGame/2"));
  CHECK(contains(request_text(request("POST"), url_of("https://example.com")), "Content-Length: 0\r\n"));
}

TEST_CASE("requests that cannot go as they are say why") {
  CHECK(request_problem(request("GET")).empty());
  CHECK(contains(request_problem(request("CONNECT")), "GET, POST"));
  CHECK(contains(request_problem(request("get")), "GET, POST"));
  CHECK(contains(request_problem(request("GET", {{"Bad Name", "x"}})), "one word"));
  CHECK(contains(request_problem(request("GET", {{"", "x"}})), "one word"));
  CHECK(contains(request_problem(request("GET", {{"X-Evil", "a\r\nHost: other"}})), "line break"));
  CHECK(contains(request_problem(request("GET", {{"X-Long", std::string(kMaxHttpHeaderValueBytes + 1, 'a')}})),
                 "longer than"));
  CHECK(contains(request_problem(request("GET", {{"Content-Length", "5"}})), "engine's to set"));
  CHECK(contains(request_problem(request("GET", {{"HOST", "other"}})), "engine's to set"));
  HttpHeaders many(kMaxHttpRequestHeaders + 1, {"X-A", "1"});
  CHECK(contains(request_problem(request("GET", many)), "at most"));
  CHECK(contains(request_problem(request("POST", {}, std::string(kMaxHttpBodyBytes + 1, 'a'))), "body"));
}

TEST_CASE("a request waits more than nothing and at most the longest timeout") {
  HttpRequest waiting = request("GET");
  waiting.timeout_seconds = 0.0f;
  CHECK(contains(request_problem(waiting), "timeout"));
  waiting.timeout_seconds = kLongestHttpTimeoutSeconds + 1.0f;
  CHECK(contains(request_problem(waiting), "timeout"));
  waiting.timeout_seconds = kLongestHttpTimeoutSeconds;
  CHECK(request_problem(waiting).empty());
}
