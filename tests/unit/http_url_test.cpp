#include <doctest/doctest.h>

#include "net/http/http_url.hpp"

using namespace redgem::net;

namespace {

constexpr std::uint16_t kHttpsPort = 443;
constexpr std::uint16_t kHttpPort = 80;
constexpr std::uint16_t kBackendPort = 4567;

Url parsed(const std::string& text) {
  std::string error;
  const auto url = parse_url(text, error);
  REQUIRE_MESSAGE(url.has_value(), error);
  return *url;
}

std::string refusal(const std::string& text) {
  std::string error;
  CHECK_FALSE(parse_url(text, error).has_value());
  return error;
}

}  // namespace

TEST_CASE("an https address gives its machine, the usual port and the path with its query") {
  const Url url = parsed("https://API.Example.com/v1/scores?top=10#ignored");
  CHECK(url.secure);
  CHECK(url.host == "api.example.com");
  CHECK(url.port == kHttpsPort);
  CHECK(url.target == "/v1/scores?top=10");
  CHECK(url.authority() == "api.example.com");
}

TEST_CASE("an address may name its port, and may have no path") {
  const Url backend = parsed("http://127.0.0.1:4567");
  CHECK_FALSE(backend.secure);
  CHECK(backend.port == kBackendPort);
  CHECK(backend.target == "/");
  CHECK(backend.authority() == "127.0.0.1:4567");
  CHECK(parsed("http://localhost?x=1").target == "/?x=1");
  CHECK(parsed("HTTP://localhost/").port == kHttpPort);
}

TEST_CASE("addresses that cannot be sent to are refused with the reason") {
  CHECK(refusal("ftp://example.com/file").find("https://") != std::string::npos);
  CHECK(refusal("example.com").find("https://") != std::string::npos);
  CHECK(refusal("https://").find("no machine") != std::string::npos);
  CHECK(refusal("https://user:secret@example.com/").find("password") != std::string::npos);
  CHECK(refusal("https://example.com:0/").find("port") != std::string::npos);
  CHECK(refusal("https://example.com:65536/").find("port") != std::string::npos);
  CHECK(refusal("https://example.com:http/").find("port") != std::string::npos);
  CHECK(refusal("https://exa_mple.com/").find("no machine") != std::string::npos);
}

TEST_CASE("nothing that could change what is sent gets into a request line") {
  CHECK(refusal("https://example.com/a b").find("spaces") != std::string::npos);
  CHECK(refusal("https://example.com/\r\nHost: evil").find("line breaks") != std::string::npos);
  CHECK(refusal(std::string("https://example.com/caf\xc3\xa9")).find("ASCII") != std::string::npos);
  CHECK(refusal("https://example.com/" + std::string(kMaxUrlBytes, 'a')).find("too long") != std::string::npos);
}

TEST_CASE("plain http goes to this machine and private networks only") {
  for (const char* near :
       {"127.0.0.1", "127.8.9.10", "10.0.0.7", "172.16.0.1", "172.31.255.255", "192.168.1.20", "169.254.3.4"}) {
    CHECK_MESSAGE(is_private_address(near), near);
  }
  for (const char* far : {"8.8.8.8", "172.15.0.1", "172.32.0.1", "192.169.0.1", "11.0.0.1", "", "localhost", "127.0.0",
                          "127.0.0.1.5", "127.0.0.256", "1..2.3"}) {
    CHECK_FALSE_MESSAGE(is_private_address(far), far);
  }
}
