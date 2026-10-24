#include <doctest/doctest.h>

#include <chrono>
#include <functional>
#include <optional>
#include <thread>

#include "http_test_server.hpp"
#include "net/http/http_client.hpp"

using namespace redgem;
using redgem::test::http_answer;
using redgem::test::HttpTestServer;

namespace {

constexpr int kOk = 200;
constexpr int kCreated = 201;
constexpr std::chrono::milliseconds kPollPause{5};
constexpr std::chrono::seconds kLongestWait{10};
constexpr float kShortTimeout = 0.3f;
constexpr std::chrono::seconds kHangUpNoticed{2};  // far less than a request's own timeout (15 s)

net::HttpRequest request(const std::string& method, const std::string& url) {
  net::HttpRequest made;
  made.method = method;
  made.url = url;
  return made;
}

int start(net::HttpClient& client, net::HttpRequest made) {
  std::string error;
  const int id = client.start(std::move(made), error);
  REQUIRE_MESSAGE(id != 0, error);
  return id;
}

// The answer to request `id`, polled for as a game does every frame.
net::HttpResponse answer(net::HttpClient& client, int id) {
  const auto give_up = std::chrono::steady_clock::now() + kLongestWait;
  while (std::chrono::steady_clock::now() < give_up) {
    for (net::HttpResult& result : client.poll()) {
      if (result.id == id) return std::move(result.response);
    }
    std::this_thread::sleep_for(kPollPause);
  }
  FAIL("no answer came");
  return {};
}

bool eventually(const std::function<bool()>& done) {
  const auto give_up = std::chrono::steady_clock::now() + kLongestWait;
  while (!done() && std::chrono::steady_clock::now() < give_up) std::this_thread::sleep_for(kPollPause);
  return done();
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("a game's request to a server on this machine gets its answer, headers and all") {
  HttpTestServer server(HttpTestServer::Kind::Plain, http_answer("200 OK", "{\"news\":\"hi\"}"));
  net::HttpClient client;
  const net::HttpResponse response = answer(client, start(client, request("GET", server.url("/news?page=2"))));
  CHECK(response.error.empty());
  CHECK(response.status == kOk);
  CHECK(response.body == "{\"news\":\"hi\"}");
  CHECK(response.headers.back() == std::make_pair(std::string("content-type"), std::string("application/json")));
  CHECK(server.last_request().rfind("GET /news?page=2 HTTP/1.1\r\n", 0) == 0);
  CHECK(client.in_flight() == 0);
}

TEST_CASE("what a request carries reaches the server as it was given") {
  HttpTestServer server(HttpTestServer::Kind::Plain, http_answer("201 Created", "{}"));
  net::HttpClient client;
  net::HttpRequest post = request("POST", server.url("/scores"));
  post.headers = {{"Authorization", "Bearer token-1"}, {"Content-Type", "application/json"}};
  post.body = "{\"points\":120}";
  CHECK(answer(client, start(client, post)).status == kCreated);
  const std::string seen = server.last_request();
  CHECK(contains(seen, "\r\nAuthorization: Bearer token-1\r\n"));
  CHECK(contains(seen, "\r\nContent-Length: 14\r\n"));
  CHECK(seen.substr(seen.size() - post.body.size()) == post.body);
}

TEST_CASE("https is spoken with a server whose certificate is trusted, and refused with one that is not") {
  HttpTestServer server(HttpTestServer::Kind::Secure, http_answer("200 OK", "secret"));
  net::HttpClient trusting(net::TrustStore::from_pem(server.certificate_pem()));
  const net::HttpResponse response = answer(trusting, start(trusting, request("GET", server.url("/"))));
  CHECK_MESSAGE(response.error.empty(), response.error);
  CHECK(response.body == "secret");

  // The system's authorities never signed a certificate made by a test: nothing is sent.
  const int requests_before = server.requests();
  net::HttpClient doubting;
  const net::HttpResponse refused = answer(doubting, start(doubting, request("GET", server.url("/"))));
  CHECK(refused.status == 0);
  CHECK(contains(refused.error, "certificate"));
  CHECK(server.requests() == requests_before);
}

// Over TLS only the server's close_notify says the connection's end is its own: an answer that
// lasts until the end, cut without one, could be cut short by anyone on the way.
TEST_CASE("an https answer that lasts until the connection ends is whole only when the server says goodbye") {
  const std::string until_the_end = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nall of it";
  HttpTestServer cut(HttpTestServer::Kind::Secure, until_the_end);
  net::HttpClient client(net::TrustStore::from_pem(cut.certificate_pem()));
  const net::HttpResponse refused = answer(client, start(client, request("GET", cut.url("/"))));
  CHECK(refused.status == 0);
  CHECK(contains(refused.error, "cut before the server said the answer was whole"));

  HttpTestServer polite(HttpTestServer::Kind::Secure, until_the_end);
  polite.say_goodbye();
  net::HttpClient trusting(net::TrustStore::from_pem(polite.certificate_pem()));
  const net::HttpResponse whole = answer(trusting, start(trusting, request("GET", polite.url("/"))));
  CHECK_MESSAGE(whole.error.empty(), whole.error);
  CHECK(whole.body == "all of it");
}

// The connection ending while the request goes (here in the TLS handshake) ends the request.
TEST_CASE("an https server that hangs up before the request went fails it at once") {
  HttpTestServer server(HttpTestServer::Kind::Secure, http_answer("200 OK", "never"));
  server.hang_up_at_once();
  net::HttpClient client(net::TrustStore::from_pem(server.certificate_pem()));
  const auto before = std::chrono::steady_clock::now();
  const net::HttpResponse refused = answer(client, start(client, request("GET", server.url("/"))));
  CHECK(refused.status == 0);
  CHECK_FALSE(refused.error.empty());
  CHECK(std::chrono::steady_clock::now() - before < kHangUpNoticed);
}

TEST_CASE("a server that never answers is given up on at the request's timeout") {
  HttpTestServer server(HttpTestServer::Kind::Plain, "");
  server.keep_quiet();
  net::HttpClient client;
  net::HttpRequest slow = request("GET", server.url("/"));
  slow.timeout_seconds = kShortTimeout;
  const auto started = std::chrono::steady_clock::now();
  const net::HttpResponse response = answer(client, start(client, slow));
  CHECK(contains(response.error, "no answer within 0.3 seconds"));
  CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(3));
}

TEST_CASE("an answer larger than a game may take is refused") {
  HttpTestServer announced(HttpTestServer::Kind::Plain, "HTTP/1.1 200 OK\r\nContent-Length: " +
                                                            std::to_string(net::kMaxHttpBodyBytes + 1) + "\r\n\r\n");
  net::HttpClient client;
  CHECK(contains(answer(client, start(client, request("GET", announced.url("/")))).error, "larger than"));

  HttpTestServer flooding(HttpTestServer::Kind::Plain,
                          "HTTP/1.1 200 OK\r\n\r\n" + std::string(net::kMaxHttpBodyBytes + 1, 'a'));
  CHECK(contains(answer(client, start(client, request("GET", flooding.url("/")))).error, "larger than"));
}

TEST_CASE("requests still on their way when the game ends are given up, and their connections closed") {
  HttpTestServer server(HttpTestServer::Kind::Plain, "");
  server.keep_quiet();
  {
    net::HttpClient client;
    start(client, request("GET", server.url("/forever")));
    REQUIRE(eventually([&server] { return server.requests() == 1; }));
  }
  CHECK(eventually([&server] { return server.saw_hang_up(); }));
}

TEST_CASE("a game has only so many requests on their way: one more is answered with an error") {
  HttpTestServer server(HttpTestServer::Kind::Plain, "");
  server.keep_quiet();
  net::HttpClient client;
  for (std::size_t i = 0; i < net::kMaxHttpRequestsInFlight; ++i) start(client, request("GET", server.url("/")));
  const net::HttpResponse refused = answer(client, start(client, request("GET", server.url("/"))));
  CHECK(contains(refused.error, "too many requests"));
  CHECK(client.in_flight() == net::kMaxHttpRequestsInFlight);
}

TEST_CASE("a request that cannot go as it is is not sent, and says why") {
  net::HttpClient client;
  std::string error;
  CHECK(client.start(request("GET", "ftp://example.com/"), error) == 0);
  CHECK(contains(error, "https://"));
  CHECK(client.start(request("BREW", "https://example.com/"), error) == 0);
  CHECK(contains(error, "GET, POST"));
  CHECK(client.in_flight() == 0);
}
