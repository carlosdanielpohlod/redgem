#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "editor/object_clipboard.hpp"

using namespace redgem;
using namespace redgem::object_clipboard;

namespace {

std::vector<std::uint8_t> bytes_of(const std::string& text) { return {text.begin(), text.end()}; }

}  // namespace

TEST_CASE("base64 encodes and decodes the RFC 4648 examples") {
  const std::pair<const char*, const char*> examples[] = {{"", ""},
                                                          {"f", "Zg=="},
                                                          {"fo", "Zm8="},
                                                          {"foo", "Zm9v"},
                                                          {"foob", "Zm9vYg=="},
                                                          {"fooba", "Zm9vYmE="},
                                                          {"foobar", "Zm9vYmFy"}};
  for (const auto& [plain, encoded] : examples) {
    CHECK(base64_encode(bytes_of(plain)) == encoded);
    const auto decoded = base64_decode(encoded);
    REQUIRE(decoded);
    CHECK(*decoded == bytes_of(plain));
  }
}

TEST_CASE("every byte value goes through base64 and back") {
  std::vector<std::uint8_t> all;
  for (int value = 0; value < 256; ++value) all.push_back(static_cast<std::uint8_t>(value));
  const auto back = base64_decode(base64_encode(all));
  REQUIRE(back);
  CHECK(*back == all);
}

TEST_CASE("text that is not base64 is refused") {
  CHECK_FALSE(base64_decode("Zm9"));       // not whole groups
  CHECK_FALSE(base64_decode("Zm9v!A=="));  // a character base64 does not use
  CHECK_FALSE(base64_decode("Z==="));      // more padding than a group can have
}

TEST_CASE("copied objects go through the clipboard's text and back") {
  const std::vector<std::uint8_t> glb = {'g', 'l', 'T', 'F', 2, 0, 0, 0, 255, 128};
  const std::string text = to_text(glb);
  const auto back = from_text(text);
  REQUIRE(back);
  CHECK(*back == glb);
}

TEST_CASE("the clipboard's text survives line ends a system adds") {
  const std::vector<std::uint8_t> glb = bytes_of("some objects copied in another window");
  std::string text = to_text(glb);
  const std::size_t line_end = text.find('\n');
  text.replace(line_end, 1, "\r\n");
  text.insert(text.size() / 2, "\r\n");
  text += "\n";
  const auto back = from_text(text);
  REQUIRE(back);
  CHECK(*back == glb);
}

TEST_CASE("other text on the clipboard holds no objects") {
  CHECK_FALSE(from_text(""));
  CHECK_FALSE(from_text("hello"));
  CHECK_FALSE(from_text("RedGem objects 1"));  // no line after the header
  CHECK_FALSE(from_text("RedGem objects 2\nZm9v"));
  CHECK_FALSE(from_text("RedGem objects 1\nnot base64!"));
}

TEST_CASE("only copies up to the limit go on the system's clipboard") {
  CHECK(fits_system_clipboard(kMaxClipboardBytes));
  CHECK_FALSE(fits_system_clipboard(kMaxClipboardBytes + 1));
}

TEST_CASE("a copy too big for the system's clipboard puts a note there, which pastes nothing") {
  constexpr std::size_t kSmallLimit = 4;
  const std::vector<std::uint8_t> small = bytes_of("tiny");
  const std::vector<std::uint8_t> big = bytes_of("too big for it");
  const std::string note = "kept in the editor";
  CHECK(from_text(system_text(small, note, kSmallLimit)) == small);
  CHECK(system_text(big, note, kSmallLimit) == note);
  CHECK_FALSE(from_text(system_text(big, note, kSmallLimit)));  // an older copy is not pasted instead
}

TEST_CASE("clipboard text longer than any copy is refused without being read") {
  const std::string text = to_text(bytes_of("some objects"));
  CHECK(from_text(text, text.size()));
  CHECK_FALSE(from_text(text, text.size() - 1));
  CHECK(kMaxClipboardTextBytes > kMaxClipboardBytes / 3 * 4);
}
