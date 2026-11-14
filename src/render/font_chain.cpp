#include "render/font_chain.hpp"

#include <SheenBidi/SheenBidi.h>

#include <sstream>
#include <string_view>

namespace redgem {
namespace {

constexpr int kFirstPrintable = 0x20;  // below it are control characters, drawn by no face
constexpr char kLineBreak = '\n';

// The first face with the letter; the last one (the built-in face in a layout) when none has it.
std::size_t first_face_with(int codepoint, std::size_t faces, const HasGlyph& has_glyph) {
  for (std::size_t face = 0; face < faces; ++face) {
    if (has_glyph(face, codepoint)) return face;
  }
  return faces == 0 ? 0 : faces - 1;
}
bool is_control(int codepoint) { return codepoint < kFirstPrintable; }
std::size_t face_before(const std::vector<FaceRun>& runs) { return runs.empty() ? 0 : runs.back().face; }
std::size_t chosen_face(const std::vector<FaceRun>& runs, int codepoint, std::size_t faces, const HasGlyph& has_glyph) {
  if (is_control(codepoint)) return face_before(runs);
  return first_face_with(codepoint, faces, has_glyph);
}
bool joins(const std::vector<FaceRun>& runs, std::size_t face) {
  return !runs.empty() && runs.back().face == face && !is_line_break(runs.back().text);
}
void add_letter(std::vector<FaceRun>& runs, std::size_t face, std::string_view bytes) {
  if (joins(runs, face))
    runs.back().text.append(bytes);
  else
    runs.push_back({face, std::string(bytes)});
}

}  // namespace

bool is_line_break(const std::string& piece) { return piece.size() == 1 && piece.front() == kLineBreak; }

std::vector<std::string> font_chain_names(const std::string& chain) {
  std::vector<std::string> names;
  std::istringstream parts(chain);
  for (std::string name; std::getline(parts, name, kFontChainSeparator);) names.push_back(name);
  return names;
}

std::string font_chain(const std::vector<std::string>& names) {
  std::string chain;
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (i > 0) chain += kFontChainSeparator;
    chain += names[i];
  }
  return chain;
}

std::vector<FaceRun> face_runs(const std::string& text, std::size_t faces, const HasGlyph& has_glyph) {
  std::vector<FaceRun> runs;
  const auto* bytes = reinterpret_cast<const SBUInt8*>(text.data());
  SBUInteger index = 0;
  while (index < text.size()) {
    const SBUInteger start = index;
    const int codepoint = static_cast<int>(SBCodepointDecodeNextFromUTF8(bytes, text.size(), &index));
    const std::string_view letter(text.data() + start, index - start);
    if (codepoint == kLineBreak)
      runs.push_back({0, std::string(letter)});
    else
      add_letter(runs, chosen_face(runs, codepoint, faces, has_glyph), letter);
  }
  return runs;
}

}  // namespace redgem
