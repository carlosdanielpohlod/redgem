#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace redgem {

// A UI font chain held as one name: the faces' names joined by a separator no font file name may
// hold (see safe_asset_name), e.g. "latin:cjk:emoji". A letter is drawn with the first face that
// has it; the built-in face is always tried after the chain.
inline constexpr char kFontChainSeparator = ':';
inline constexpr std::size_t kMaxChainFaces = 4;  // game faces in one chain (the built-in one aside)

std::vector<std::string> font_chain_names(const std::string& chain);
std::string font_chain(const std::vector<std::string>& names);

// A piece of a text that one face draws, by its place in the chain. A line break is a piece of its own.
struct FaceRun {
  std::size_t face = 0;
  std::string text;
};
bool is_line_break(const std::string& piece);
using HasGlyph = std::function<bool(std::size_t face, int codepoint)>;

// The text cut where the face that draws it changes: each letter goes to the first of `faces`
// faces that has it, or to the last face (the built-in one, which shows its "?") when none has it.
// Control characters other than line breaks stay with the letter before them.
std::vector<FaceRun> face_runs(const std::string& text, std::size_t faces, const HasGlyph& has_glyph);

}  // namespace redgem
