#include <doctest/doctest.h>

#include <cstddef>
#include <string>

#include "render/bidi_text.hpp"

using namespace redgem;

namespace {
constexpr const char* kShalom = "שלום";       // Hebrew, as typed (logical order)
constexpr const char* kShalomDrawn = "םולש";  // the same letters, left to right on screen
constexpr const char* kArabicMarked = "بَت";   // beh with a fatha above it, then teh
constexpr const char* kArabicDrawn = "تبَ";    // teh, then beh still carrying its fatha
}  // namespace

TEST_CASE("left-to-right text is drawn as written") {
  CHECK(visual_order("Score: 120 (best)") == "Score: 120 (best)");
  CHECK(visual_order("Żółć € Ω") == "Żółć € Ω");
  CHECK(visual_order("").empty());
}

TEST_CASE("a right-to-left word is drawn reversed") { CHECK(visual_order(kShalom) == kShalomDrawn); }

TEST_CASE("a right-to-left word inside a left-to-right line is reversed in place") {
  CHECK(visual_order(std::string("abc ") + kShalom + " def") == std::string("abc ") + kShalomDrawn + " def");
}

TEST_CASE("a right-to-left line keeps its Latin words and numbers readable, in right-to-left order") {
  CHECK(visual_order(std::string(kShalom) + " 123") == std::string("123 ") + kShalomDrawn);
  // (Digits after a Latin word belong to it: "abc 123" stays together, left to right.)
  CHECK(visual_order(std::string(kShalom) + " abc 123") == std::string("abc 123 ") + kShalomDrawn);
}

TEST_CASE("brackets in right-to-left text are mirrored") {
  CHECK(visual_order(std::string("(") + kShalom + ")") == std::string("(") + kShalomDrawn + ")");
}

TEST_CASE("marks stay on the letter they belong to") { CHECK(visual_order(kArabicMarked) == kArabicDrawn); }

TEST_CASE("emoji joined into one picture stay whole inside right-to-left text") {
  constexpr const char* kFamily = "👨‍👩";  // man, zero width joiner, woman
  CHECK(visual_order(std::string(kShalom) + " " + kFamily) == std::string(kFamily) + " " + kShalomDrawn);
}

TEST_CASE("each line takes its own direction and keeps its line break") {
  const std::string text = std::string(kShalom) + "\nabc " + kShalom;
  CHECK(visual_order(text) == std::string(kShalomDrawn) + "\nabc " + kShalomDrawn);
  CHECK(visual_order(std::string(kShalom) + "\n\n") == std::string(kShalomDrawn) + "\n\n");
}

TEST_CASE("very long texts are drawn as written, without ordering work") {
  constexpr std::size_t kLongRepeats = 20000;
  std::string text;
  for (std::size_t i = 0; i < kLongRepeats; ++i) text += kShalom;
  CHECK(visual_order(text) == text);
}
