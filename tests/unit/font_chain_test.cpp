#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "render/font_chain.hpp"

using namespace redgem;

namespace {
constexpr std::size_t kLatin = 0;
constexpr std::size_t kCjk = 1;
constexpr std::size_t kEmoji = 2;
constexpr std::size_t kChainFaces = 3;
constexpr int kLatinEnd = 0x250;  // Basic Latin up to Latin Extended-B
constexpr int kCjkFirst = 0x4e00;
constexpr int kCjkLast = 0x9fff;
constexpr int kEmojiFirst = 0x1f300;
constexpr int kEmojiLast = 0x1faff;

bool in_range(int codepoint, int first, int last) { return codepoint >= first && codepoint <= last; }
// A Latin face, a CJK face and an emoji face, each with only its own letters.
bool covers(std::size_t face, int codepoint) {
  if (face == kLatin) return codepoint < kLatinEnd;
  if (face == kCjk) return in_range(codepoint, kCjkFirst, kCjkLast);
  return in_range(codepoint, kEmojiFirst, kEmojiLast);
}
std::vector<FaceRun> runs_of(const std::string& text) { return face_runs(text, kChainFaces, covers); }
bool same_run(const FaceRun& run, const FaceRun& expected) {
  return run.face == expected.face && run.text == expected.text;
}
bool same(const std::vector<FaceRun>& runs, const std::vector<FaceRun>& expected) {
  if (runs.size() != expected.size()) return false;
  for (std::size_t i = 0; i < runs.size(); ++i) {
    if (!same_run(runs[i], expected[i])) return false;
  }
  return true;
}
}  // namespace

TEST_CASE("a font chain is held as one name and read back face by face") {
  const std::vector<std::string> names{"latin", "cjk", "emoji"};
  CHECK(font_chain(names) == "latin:cjk:emoji");
  CHECK(font_chain_names("latin:cjk:emoji") == names);
  CHECK(font_chain_names("latin") == std::vector<std::string>{"latin"});
  CHECK(font_chain_names("").empty());
}

TEST_CASE("each letter is drawn with the first face of the chain that has it") {
  CHECK(same(runs_of("Hi 日本😀!"), {{kLatin, "Hi "}, {kCjk, "日本"}, {kEmoji, "😀"}, {kLatin, "!"}}));
  CHECK(same(runs_of("日本語"), {{kCjk, "日本語"}}));
}

TEST_CASE("a letter no face has goes to the first face, which shows its fallback glyph") {
  CHECK(same(runs_of("aΩb"), {{kLatin, "aΩb"}}));
  CHECK(same(runs_of("日Ω"), {{kCjk, "日"}, {kLatin, "Ω"}}));
}

TEST_CASE("earlier faces win when several have a letter") {
  const auto everything = [](std::size_t, int) { return true; };
  const auto runs = face_runs("A日😀", kChainFaces, everything);
  CHECK(same(runs, {{kLatin, "A日😀"}}));
}

TEST_CASE("line breaks are pieces of their own and other control characters stay with their letter") {
  CHECK(same(runs_of("日\n本"), {{kCjk, "日"}, {kLatin, "\n"}, {kCjk, "本"}}));
  CHECK(same(runs_of("日\t本"), {{kCjk, "日\t本"}}));
  CHECK(is_line_break("\n"));
  CHECK_FALSE(is_line_break("a\n"));
}

TEST_CASE("a single face draws the whole text") { CHECK(same(face_runs("Hi 日本", 1, covers), {{kLatin, "Hi 日本"}})); }
