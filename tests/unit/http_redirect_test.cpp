#include <doctest/doctest.h>

#include <algorithm>

#include "net/http/http_message.hpp"
#include "net/http/http_redirect.hpp"

using namespace redgem::net;

namespace {

constexpr int kOk = 200;
constexpr int kMovedPermanently = 301;
constexpr int kFound = 302;
constexpr int kSeeOther = 303;
constexpr int kNotModified = 304;
constexpr int kTemporaryRedirect = 307;
constexpr int kPermanentRedirect = 308;

Url parsed(const std::string& text) {
  std::string error;
  const auto url = parse_url(text, error);
  REQUIRE_MESSAGE(url.has_value(), error);
  return *url;
}

// Where `location` sends a request made to `from`, as text.
std::string target(const std::string& from, const std::string& location) {
  std::string error;
  const auto to = redirect_target(parsed(from), location, error);
  REQUIRE_MESSAGE(to.has_value(), error);
  return url_text(*to);
}

std::string refusal(const std::string& from, const std::string& location) {
  std::string error;
  CHECK_FALSE(redirect_target(parsed(from), location, error).has_value());
  return error;
}

HttpResponse answer(int status, const std::string& location) {
  HttpResponse response;
  response.status = status;
  response.headers = {{"location", location}};
  return response;
}

HttpRequest post_with_credentials() {
  HttpRequest request;
  request.method = "POST";
  request.url = "https://api.example.com/scores";
  request.headers = {{"Authorization", "Bearer token"},
                     {"Cookie", "session=1"},
                     {"Content-Type", "application/json"},
                     {"X-Game", "coins"}};
  request.body = "{\"points\":120}";
  return request;
}

bool has(const HttpRequest& request, const std::string& lower_name) {
  return std::any_of(request.headers.begin(), request.headers.end(),
                     [&lower_name](const auto& header) { return lower_case(header.first) == lower_name; });
}

}  // namespace

TEST_CASE("the redirect statuses with a Location are followed, and nothing else") {
  for (const int status : {kMovedPermanently, kFound, kSeeOther, kTemporaryRedirect, kPermanentRedirect}) {
    CHECK_MESSAGE(is_followed_redirect(answer(status, "/next")), status);
  }
  CHECK_FALSE(is_followed_redirect(answer(kOk, "/next")));
  CHECK_FALSE(is_followed_redirect(answer(kNotModified, "/next")));
  CHECK_FALSE(is_followed_redirect(answer(kFound, "")));
  HttpResponse failed = answer(kFound, "/next");
  failed.error = "cut short";
  CHECK_FALSE(is_followed_redirect(failed));
}

TEST_CASE("a Location is read against the address its answer came from") {
  const std::string from = "https://api.example.com/v1/scores/top?page=2";
  CHECK(target(from, "https://cdn.example.com/a") == "https://cdn.example.com/a");
  CHECK(target(from, "//cdn.example.com:8443/a") == "https://cdn.example.com:8443/a");
  CHECK(target(from, "/v2/scores") == "https://api.example.com/v2/scores");
  CHECK(target(from, "best") == "https://api.example.com/v1/scores/best");
  CHECK(target(from, "../players?id=3") == "https://api.example.com/v1/players?id=3");
  CHECK(target(from, "./") == "https://api.example.com/v1/scores/");
  CHECK(target(from, "../../../../up") == "https://api.example.com/up");
  CHECK(target(from, "?page=3") == "https://api.example.com/v1/scores/top?page=3");
  CHECK(target(from, "/a/b/../c/./d#part") == "https://api.example.com/a/c/d");
  CHECK(target("http://127.0.0.1:4567/x", "/y") == "http://127.0.0.1:4567/y");
  CHECK(target(from, "  /spaced  ") == "https://api.example.com/spaced");
}

TEST_CASE("a redirect never goes from https to http, nor anywhere a request may not go") {
  CHECK(refusal("https://api.example.com/", "http://api.example.com/").find("https:// to http://") !=
        std::string::npos);
  CHECK(refusal("https://api.example.com/", "ftp://files.example.com/").find("https://") != std::string::npos);
  CHECK(refusal("https://api.example.com/", "https://user:pw@evil.example.com/").find("password") != std::string::npos);
  CHECK(refusal("https://api.example.com/", "/a b").find("spaces") != std::string::npos);
  CHECK(refusal("https://api.example.com/", "javascript:alert(1)").find("https://") != std::string::npos);
  // From http, anything a request may go to: the machine checks still run when it is sent.
  CHECK(target("http://127.0.0.1/", "https://api.example.com/") == "https://api.example.com/");
}

TEST_CASE("a 303, and a 301 or 302 to a POST, become a GET without the body") {
  const Url from = parsed("https://api.example.com/scores");
  const Url to = parsed("https://api.example.com/done");
  for (const int status : {kSeeOther, kMovedPermanently, kFound}) {
    const HttpRequest next = redirected(post_with_credentials(), status, from, to);
    CHECK(next.method == "GET");
    CHECK(next.body.empty());
    CHECK_FALSE(has(next, "content-type"));
    CHECK(has(next, "x-game"));
    CHECK(next.url == "https://api.example.com/done");
  }
  HttpRequest put = post_with_credentials();
  put.method = "PUT";
  CHECK(redirected(put, kMovedPermanently, from, to).method == "PUT");
  CHECK(redirected(put, kSeeOther, from, to).method == "GET");
}

TEST_CASE("a 307 or 308 keeps the method and the body") {
  const Url from = parsed("https://api.example.com/scores");
  const Url to = parsed("https://api.example.com/v2/scores");
  for (const int status : {kTemporaryRedirect, kPermanentRedirect}) {
    const HttpRequest next = redirected(post_with_credentials(), status, from, to);
    CHECK(next.method == "POST");
    CHECK(next.body == "{\"points\":120}");
    CHECK(has(next, "content-type"));
  }
}

TEST_CASE("credentials go to the origin they were given to, and no further") {
  const Url from = parsed("https://api.example.com/scores");
  const HttpRequest same =
      redirected(post_with_credentials(), kTemporaryRedirect, from, parsed("https://api.example.com/x"));
  CHECK(has(same, "authorization"));
  CHECK(has(same, "cookie"));
  for (const char* elsewhere :
       {"https://evil.example.com/x", "https://api.example.com:8443/x", "http://api.example.com/x"}) {
    const HttpRequest next = redirected(post_with_credentials(), kTemporaryRedirect, from, parsed(elsewhere));
    CHECK_FALSE_MESSAGE(has(next, "authorization"), elsewhere);
    CHECK_FALSE_MESSAGE(has(next, "cookie"), elsewhere);
    CHECK_MESSAGE(has(next, "x-game"), elsewhere);
  }
}

TEST_CASE("following a redirect moves the request on, and says why when it cannot") {
  HttpRequest request = post_with_credentials();
  Url url = parsed(request.url);
  std::string error;
  REQUIRE(follow_redirect(request, url, answer(kSeeOther, "/thanks"), error));
  CHECK(url_text(url) == "https://api.example.com/thanks");
  CHECK(request.method == "GET");
  CHECK_FALSE(follow_redirect(request, url, answer(kFound, "http://api.example.com/"), error));
  CHECK(error.find("cannot be followed") != std::string::npos);
  CHECK(url_text(url) == "https://api.example.com/thanks");
}
