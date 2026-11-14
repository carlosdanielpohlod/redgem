#include <doctest/doctest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>

#include "fake_clock.hpp"
#include "http_compression.hpp"
#include "http_test_server.hpp"
#include "net/http/http_client.hpp"
#include "net/http/http_message.hpp"
#include "net/http/http_redirect.hpp"
#include "net/http/http_stream.hpp"
#include "net/socket/socket.hpp"

using namespace redgem;
using redgem::test::http_answer;
using redgem::test::HttpTestServer;

namespace {

constexpr int kOk = 200;
constexpr int kCreated = 201;
constexpr int kFound = 302;
constexpr std::chrono::milliseconds kPollPause{5};
constexpr std::chrono::seconds kLongestWait{10};
constexpr float kShortTimeout = 0.3f;
constexpr int kTimeoutSteps = 10;  // the fake clock passes a short timeout in this many polls

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

// The answer to request `id`, polled for as a game does every frame; `between` runs between polls.
net::HttpResponse answer(net::HttpClient& client, int id, const std::function<void()>& between = [] {}) {
  const auto give_up = std::chrono::steady_clock::now() + kLongestWait;
  while (std::chrono::steady_clock::now() < give_up) {
    for (net::HttpResult& result : client.poll()) {
      if (result.id == id) return std::move(result.response);
    }
    between();
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
  // On a clock that never moves, its timeout never comes: only the hang-up can end the request.
  const auto clock = std::make_shared<test::FakeClock>();
  net::HttpClient client(net::TrustStore::from_pem(server.certificate_pem()), clock);
  const net::HttpResponse refused = answer(client, start(client, request("GET", server.url("/"))));
  CHECK(refused.status == 0);
  CHECK_FALSE(refused.error.empty());
  CHECK_FALSE(contains(refused.error, "no answer within"));
}

TEST_CASE("a server that never answers is given up on at the request's timeout") {
  HttpTestServer server(HttpTestServer::Kind::Plain, "");
  server.keep_quiet();
  const auto clock = std::make_shared<test::FakeClock>();
  net::HttpClient client(nullptr, clock);
  net::HttpRequest slow = request("GET", server.url("/"));
  slow.timeout_seconds = kShortTimeout;
  const double started = clock->now();
  const net::HttpResponse response =
      answer(client, start(client, slow), [&clock] { clock->advance(kShortTimeout / kTimeoutSteps); });
  CHECK(contains(response.error, "no answer within 0.3 seconds"));
  // Given up before the clock got anywhere near the default timeout: at the request's own.
  CHECK(clock->now() - started < net::kDefaultHttpTimeoutSeconds);
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

// --- Redirects ---------------------------------------------------------------------------------

namespace {

std::string redirect_to(const std::string& status, const std::string& location) {
  return http_answer(status, "", "Location: " + location + "\r\n");
}

const std::string kPoints = "{\"points\":120}";

net::HttpRequest post_with_credentials(const std::string& url) {
  net::HttpRequest post = request("POST", url);
  post.headers = {{"Authorization", "Bearer token-1"},
                  {"Cookie", "session=7"},
                  {"Content-Type", "application/json"},
                  {"X-Game", "coins"}};
  post.body = kPoints;
  return post;
}

bool ends_with(const std::string& text, const std::string& end) {
  return text.size() >= end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
}

}  // namespace

TEST_CASE("redirects are followed from server to server, and the answer says where it came from") {
  HttpTestServer last(HttpTestServer::Kind::Plain, http_answer("200 OK", "the end"));
  HttpTestServer middle(HttpTestServer::Kind::Plain, redirect_to("302 Found", last.url("/end?from=middle")));
  HttpTestServer first(HttpTestServer::Kind::Plain, redirect_to("301 Moved Permanently", middle.url("/middle")));
  net::HttpClient client;
  const net::HttpResponse response = answer(client, start(client, request("GET", first.url("/start"))));
  CHECK_MESSAGE(response.error.empty(), response.error);
  CHECK(response.status == kOk);
  CHECK(response.body == "the end");
  CHECK(response.url == last.url("/end?from=middle"));
  CHECK(middle.last_request().rfind("GET /middle HTTP/1.1\r\n", 0) == 0);
  CHECK(last.last_request().rfind("GET /end?from=middle HTTP/1.1\r\n", 0) == 0);
  CHECK(first.requests() == 1);
  CHECK(middle.requests() == 1);
}

TEST_CASE("a relative redirect is read against the address, and a 307 keeps the method, body and credentials") {
  HttpTestServer server(HttpTestServer::Kind::Plain, "");
  server.answer_in_turn({redirect_to("307 Temporary Redirect", "next?try=2"), http_answer("201 Created", "{}")});
  net::HttpClient client;
  const net::HttpResponse response = answer(client, start(client, post_with_credentials(server.url("/v1/start"))));
  CHECK_MESSAGE(response.error.empty(), response.error);
  CHECK(response.status == kCreated);
  CHECK(response.url == server.url("/v1/next?try=2"));
  const std::string seen = server.last_request();
  CHECK(seen.rfind("POST /v1/next?try=2 HTTP/1.1\r\n", 0) == 0);
  CHECK(contains(seen, "\r\nAuthorization: Bearer token-1\r\n"));
  CHECK(contains(seen, "\r\nCookie: session=7\r\n"));
  CHECK(ends_with(seen, kPoints));
}

TEST_CASE("a redirect to another origin drops the credentials, and a 302 to a POST becomes a GET") {
  HttpTestServer landing(HttpTestServer::Kind::Plain, http_answer("200 OK", "{}"));
  HttpTestServer server(HttpTestServer::Kind::Plain, redirect_to("302 Found", landing.url("/landing")));
  net::HttpClient client;
  const net::HttpResponse response = answer(client, start(client, post_with_credentials(server.url("/scores"))));
  CHECK_MESSAGE(response.error.empty(), response.error);
  const std::string seen = landing.last_request();
  CHECK(seen.rfind("GET /landing HTTP/1.1\r\n", 0) == 0);
  CHECK_FALSE(contains(seen, "Authorization"));
  CHECK_FALSE(contains(seen, "Cookie"));
  CHECK_FALSE(contains(seen, "Content-Type"));
  CHECK(contains(seen, "\r\nX-Game: coins\r\n"));
  CHECK(ends_with(seen, "\r\n\r\n"));  // no body
}

TEST_CASE("a request follows so many redirects, and the next one is an error") {
  HttpTestServer server(HttpTestServer::Kind::Plain, redirect_to("302 Found", "/again"));
  net::HttpClient client;
  const net::HttpResponse response = answer(client, start(client, request("GET", server.url("/"))));
  CHECK(response.status == 0);
  CHECK(contains(response.error, "more than " + std::to_string(net::kMaxHttpRedirects)));
  CHECK(response.url == server.url("/again"));
  CHECK(server.requests() == net::kMaxHttpRedirects + 1);
}

TEST_CASE("a request that does not follow redirects gets the redirect itself") {
  HttpTestServer landing(HttpTestServer::Kind::Plain, http_answer("200 OK", "{}"));
  HttpTestServer server(HttpTestServer::Kind::Plain, redirect_to("302 Found", "myapp://login?code=7"));
  net::HttpClient client;
  net::HttpRequest get = request("GET", server.url("/authorize"));
  get.follow_redirects = false;
  const net::HttpResponse response = answer(client, start(client, get));
  CHECK_MESSAGE(response.error.empty(), response.error);
  CHECK(response.status == kFound);
  CHECK(net::header_value(response.headers, "location") == "myapp://login?code=7");
  CHECK(response.url == server.url("/authorize"));
  CHECK(server.requests() == 1);
}

TEST_CASE("a redirect from https to http is not followed: nothing goes out in plain text") {
  HttpTestServer plain(HttpTestServer::Kind::Plain, http_answer("200 OK", "{}"));
  HttpTestServer secure(HttpTestServer::Kind::Secure, redirect_to("302 Found", plain.url("/")));
  net::HttpClient client(net::TrustStore::from_pem(secure.certificate_pem()));
  const net::HttpResponse response = answer(client, start(client, request("GET", secure.url("/"))));
  CHECK(response.status == 0);
  CHECK(contains(response.error, "https:// to http://"));
  CHECK(plain.requests() == 0);
}

// A redirect is no way around what a game's requests may reach.
TEST_CASE("every address a redirect leads to is checked as the request's own was") {
  net::HttpClient client;
  HttpTestServer metadata(HttpTestServer::Kind::Plain,
                          redirect_to("302 Found", "http://169.254.169.254/latest/meta-data/iam/"));
  CHECK(contains(answer(client, start(client, request("GET", metadata.url("/")))).error, "metadata address"));

  HttpTestServer other_scheme(HttpTestServer::Kind::Plain, redirect_to("301 Moved Permanently", "ftp://example.com/"));
  CHECK(contains(answer(client, start(client, request("GET", other_scheme.url("/")))).error, "https://"));

  HttpTestServer credentials(HttpTestServer::Kind::Plain, redirect_to("302 Found", "http://user:pw@127.0.0.1/"));
  CHECK(contains(answer(client, start(client, request("GET", credentials.url("/")))).error, "password"));
}

// --- Compressed answers -------------------------------------------------------------------------

namespace {

constexpr std::size_t kNewsLetters = 4000;

std::string news() { return "{\"news\":\"" + std::string(kNewsLetters, 'n') + "\"}"; }

// `bytes` sent in two chunks.
std::string chunked(const std::string& bytes) {
  const std::size_t half = bytes.size() / 2;
  std::ostringstream out;
  out << std::hex << half << "\r\n" << bytes.substr(0, half) << "\r\n";
  out << (bytes.size() - half) << "\r\n" << bytes.substr(half) << "\r\n0\r\n\r\n";
  return out.str();
}

}  // namespace

TEST_CASE("requests ask for gzip or deflate, and such answers reach the game decompressed") {
  const std::pair<std::string, int> encodings[] = {{"gzip", test::kGzipWrapper}, {"deflate", test::kZlibWrapper}};
  for (const auto& [name, window] : encodings) {
    HttpTestServer server(HttpTestServer::Kind::Plain, http_answer("200 OK", test::compressed(news(), window),
                                                                   "Content-Encoding: " + name + "\r\n"));
    net::HttpClient client;
    const net::HttpResponse response = answer(client, start(client, request("GET", server.url("/news"))));
    CHECK_MESSAGE(response.error.empty(), response.error);
    CHECK(response.body == news());
    CHECK(contains(server.last_request(), "\r\nAccept-Encoding: gzip, deflate\r\n"));
  }
}

TEST_CASE("a game that names its own Accept-Encoding sends only that one") {
  HttpTestServer server(HttpTestServer::Kind::Plain, http_answer("200 OK", "plain"));
  net::HttpClient client;
  net::HttpRequest get = request("GET", server.url("/"));
  get.headers = {{"Accept-Encoding", "identity"}};
  CHECK(answer(client, start(client, get)).body == "plain");
  CHECK(contains(server.last_request(), "\r\nAccept-Encoding: identity\r\n"));
  CHECK_FALSE(contains(server.last_request(), "gzip"));
}

TEST_CASE("a gzip answer sent in chunks is put together, then decompressed") {
  HttpTestServer server(HttpTestServer::Kind::Plain,
                        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Encoding: gzip\r\n\r\n" +
                            chunked(test::compressed(news(), test::kGzipWrapper)));
  net::HttpClient client;
  const net::HttpResponse response = answer(client, start(client, request("GET", server.url("/"))));
  CHECK_MESSAGE(response.error.empty(), response.error);
  CHECK(response.body == news());
}

TEST_CASE("a compressed answer that would outgrow the limit, or is corrupt, is refused") {
  const std::string bomb = test::compressed(std::string(net::kMaxHttpDecodedBodyBytes + 1, '0'), test::kGzipWrapper);
  HttpTestServer bombing(HttpTestServer::Kind::Plain, http_answer("200 OK", bomb, "Content-Encoding: gzip\r\n"));
  net::HttpClient client;
  const net::HttpResponse refused = answer(client, start(client, request("GET", bombing.url("/"))));
  CHECK(refused.status == kOk);  // (the answer came: its status and headers stay)
  CHECK(contains(refused.error, "once decompressed"));
  CHECK(refused.body.empty());

  HttpTestServer corrupt(HttpTestServer::Kind::Plain,
                         http_answer("200 OK", "not gzip at all", "Content-Encoding: gzip\r\n"));
  const net::HttpResponse broken = answer(client, start(client, request("GET", corrupt.url("/"))));
  CHECK(broken.status == kOk);
  CHECK(contains(broken.error, "gzip body is corrupt"));
  CHECK(broken.body.empty());
}

TEST_CASE("a game that ends while a request waits on a slow server ends at once, and its thread is joined") {
  // (Its timeout is far off: only giving the request up ends it.)
  HttpTestServer server(HttpTestServer::Kind::Plain, "");
  server.keep_quiet();
  const auto clock = std::make_shared<test::FakeClock>();  // (which never moves: no timeout comes)
  auto client = std::make_unique<net::HttpClient>(nullptr, clock);
  net::HttpRequest slow = request("GET", server.url("/slow"));
  slow.timeout_seconds = net::kLongestHttpTimeoutSeconds;
  start(*client, slow);
  REQUIRE(eventually([&server] { return server.requests() == 1; }));
  const auto ending = std::chrono::steady_clock::now();
  client.reset();
  CHECK(std::chrono::steady_clock::now() - ending < net::kHttpShutdownWait);
  CHECK(eventually([&server] { return server.saw_hang_up(); }));
}

TEST_CASE("requests answered just as their game ends, collected or not, end with it safely") {
  // (Each game ends a little later than the one before: some answers came, some are on their way.)
  constexpr int kGames = 20;
  constexpr std::chrono::microseconds kLaterEachGame{250};
  HttpTestServer server(HttpTestServer::Kind::Plain, http_answer("200 OK", "{}"));
  for (int game = 0; game < kGames; ++game) {
    const auto ending = std::chrono::steady_clock::now();
    {
      net::HttpClient client;
      start(client, request("GET", server.url("/")));
      std::this_thread::sleep_for(kLaterEachGame * game);
      if (game % 2 == 0) client.poll();  // an answer collected and its thread joined, or not yet
    }
    CHECK(std::chrono::steady_clock::now() - ending < net::kHttpShutdownWait + kLaterEachGame * game);
  }
}

// (Windows lets a wait on a socket hung up from another thread sleep on: there, a request given
// up is seen at its wait's next look instead.)
#ifndef _WIN32
namespace {

// A connection to `listener`, made.
net::Socket connected_to(const net::Socket& listener) {
  std::string error;
  net::Socket socket = net::Socket::connect_to(net::kThisMachine, listener.local_port(), error);
  REQUIRE_MESSAGE(socket.valid(), error);
  REQUIRE(eventually([&socket] { return socket.connect_status() == net::IoStatus::Ok; }));
  return socket;
}

}  // namespace

TEST_CASE("giving a request up hangs up the connection it waits on, so the wait ends at once") {
  constexpr float kLongWaitSeconds = 10.0f;  // (far past the bound checked below)
  constexpr std::chrono::milliseconds kWhileItWaits{100};
  std::string error;
  const net::Socket listener = net::Socket::listen_local(0, error);
  REQUIRE_MESSAGE(listener.valid(), error);
  net::Socket socket = connected_to(listener);
  net::HttpCancel cancel;
  const auto started = std::chrono::steady_clock::now();
  bool woken = false;
  std::thread waiting([&] {
    const net::HeldConnection held(cancel, socket);
    woken = net::Socket::wait({{&socket, false}}, kLongWaitSeconds);
  });
  std::this_thread::sleep_for(kWhileItWaits);
  cancel.cancel();
  waiting.join();
  CHECK(woken);
  CHECK(std::chrono::steady_clock::now() - started < net::kHttpShutdownWait);
  // Given up before it waits, a connection is hung up as soon as it is held.
  net::Socket later = connected_to(listener);
  const net::HeldConnection held(cancel, later);
  CHECK(net::Socket::wait({{&later, false}}, kLongWaitSeconds));
}
#endif
