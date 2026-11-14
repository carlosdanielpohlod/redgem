#include <doctest/doctest.h>

#include <set>
#include <string>

#include "net/resume_token.hpp"
#include "net/wire.hpp"

using namespace redgem::net;
using nlohmann::json;

namespace {

constexpr int kManyTokens = 64;
constexpr const char* kHexDigits = "0123456789abcdef";
constexpr float kPastMostWindow = kMostReconnectWindowSeconds + 1.0f;
constexpr float kSomeWindow = 20.0f;
constexpr int kGuest = 3;

}  // namespace

TEST_CASE("a resume token is hexadecimal of its length, and never the same twice") {
  std::set<std::string> seen;
  for (int i = 0; i < kManyTokens; ++i) {
    const std::string token = new_resume_token();
    REQUIRE(token.size() == kResumeTokenDigits);
    CHECK(token.find_first_not_of(kHexDigits) == std::string::npos);
    seen.insert(token);
  }
  CHECK(seen.size() == static_cast<std::size_t>(kManyTokens));
}

TEST_CASE("a resume token matches only itself, and an empty one matches nothing") {
  const std::string token = new_resume_token();
  std::string one_off = token;
  one_off.back() = one_off.back() == 'a' ? 'b' : 'a';
  CHECK(same_resume_token(token, token));
  CHECK_FALSE(same_resume_token(one_off, token));
  CHECK_FALSE(same_resume_token(token.substr(1), token));
  CHECK_FALSE(same_resume_token(token + "0", token));
  CHECK_FALSE(same_resume_token("", ""));
  CHECK_FALSE(same_resume_token(token, ""));
}

TEST_CASE("a hello that resumes carries its token, and a welcome its token and window") {
  const json hello = wire::resume_hello(new_resume_token());
  CHECK(hello[wire::kKind] == wire::kHello);
  CHECK(wire::well_formed(hello));
  CHECK(wire::speaks_our_protocol(hello));
  const json welcome = {{wire::kKind, wire::kWelcome},
                        {wire::kTo, kGuest},
                        {wire::kResume, new_resume_token()},
                        {wire::kWindow, kSomeWindow}};
  CHECK(wire::well_formed(welcome));
}

TEST_CASE("a token longer than one, or not a text, and a window that cannot be, are not well formed") {
  const std::string too_long(kResumeTokenDigits + 1, 'a');
  CHECK_FALSE(wire::well_formed({{wire::kKind, wire::kHello}, {wire::kResume, too_long}}));
  CHECK_FALSE(wire::well_formed({{wire::kKind, wire::kHello}, {wire::kResume, kGuest}}));
  CHECK_FALSE(wire::well_formed({{wire::kKind, wire::kWelcome}, {wire::kWindow, -1}}));
  CHECK_FALSE(wire::well_formed({{wire::kKind, wire::kWelcome}, {wire::kWindow, kPastMostWindow}}));
  CHECK_FALSE(wire::well_formed({{wire::kKind, wire::kWelcome}, {wire::kWindow, "soon"}}));
}

TEST_CASE("the words of reconnecting say their kind, and whom they are about") {
  CHECK(wire::word(wire::kBye) == json{{wire::kKind, wire::kBye}});
  const json back = wire::word_about(wire::kBack, kGuest);
  CHECK(back[wire::kKind] == wire::kBack);
  CHECK(back[wire::kFrom] == kGuest);
  CHECK(wire::well_formed(back));
}
