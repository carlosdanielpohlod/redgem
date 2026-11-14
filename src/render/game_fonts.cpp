#include "render/game_fonts.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cstddef>
#include <utility>

#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "render/bidi_text.hpp"
#include "render/font_chain.hpp"
#include "render/font_rasterizer.hpp"
#include "render/ui_font.hpp"

namespace redgem {
namespace {

constexpr int kFontPixels = 72;  // same raster size as the built-in UI font
constexpr int kFirstLetter = 32;
constexpr int kLastInitialLetter = 255;
constexpr std::size_t kMaxFontFaces = 5;  // built-in face and four game fonts
constexpr std::size_t kMaxFontLetters = 2048;
constexpr std::size_t kMaxFontBytes = 32u * 1024u * 1024u;
constexpr const char* kFontExtensions[] = {"", ".ttf", ".otf"};
constexpr const char* kFontType = ".ttf";  // raylib reads both TrueType and OpenType through this decoder
constexpr float kLineGap = 2.0f;           // raylib's own space between the lines of one text
std::vector<std::uint8_t> read_font(const std::filesystem::path& folder, const std::string& name) {
  const auto file = find_asset_file(folder, name, kFontExtensions);
  if (!file) return {};
  auto bytes = read_file_up_to(*file, kMaxFontBytes);
  if (!bytes) return {};
  return std::move(*bytes);
}
bool owns_font(const Font& font) { return font.glyphs != nullptr && font.glyphs != GetFontDefault().glyphs; }
bool usable_font(const Font& font) { return IsFontValid(font) && owns_font(font); }
bool visible_fallback(const Font& font) {
  constexpr int kFallbackLetter = 63;
  if (!IsFontValid(font)) return false;
  const auto glyph = GetGlyphInfo(font, kFallbackLetter);
  const auto rect = GetGlyphAtlasRec(font, kFallbackLetter);
  return glyph.value == kFallbackLetter && rect.width > 0 && rect.height > 0;
}
bool empty_glyph(const GlyphInfo& glyph) { return glyph.image.width == 0 && glyph.advanceX == 0; }
void prepare_atlas(Font& font) {
  constexpr int kNoBreakSpace = 0xa0;
  // Absent glyphs otherwise have a codepoint but no pixels or advance: the letter disappears.
  for (int i = 0; i < font.glyphCount; ++i) {
    auto& glyph = font.glyphs[i];
    if (glyph.value == kNoBreakSpace) glyph.advanceX = GetGlyphInfo(font, kFirstLetter).advanceX;
    if (empty_glyph(glyph)) glyph.value = 0;
  }
  GenTextureMipmaps(&font.texture);
  SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
}
Font rasterize_face(const std::vector<std::uint8_t>& bytes, const std::set<int>& codes) {
  if (!bytes.empty()) return rasterize_game_font(bytes, codes, kFontPixels);
  const EmbeddedFile embedded = embedded_ui_font();
  std::vector<int> letters(codes.begin(), codes.end());
  return LoadFontFromMemory(kFontType, embedded.data, embedded.size, kFontPixels, letters.data(),
                            static_cast<int>(letters.size()));
}

// A chain's faces by name: at most kMaxChainFaces game faces, then the built-in one.
std::vector<std::string> chain_faces(const std::string& chain) {
  std::vector<std::string> names = font_chain_names(chain);
  if (names.size() > kMaxChainFaces) names.resize(kMaxChainFaces);
  names.emplace_back();
  return names;
}
void place_pieces(std::vector<FontPiece>& pieces, float size, float spacing) {
  Vector2 pen{};
  for (FontPiece& piece : pieces) {
    piece.at = pen;
    if (piece.font == nullptr) {
      pen = {0.0f, pen.y + size + kLineGap};
      continue;
    }
    piece.width = MeasureTextEx(*piece.font, piece.text.c_str(), size, spacing).x;
    pen.x += piece.width + spacing;
  }
}

}  // namespace

float laid_out_width(const std::vector<FontPiece>& pieces) {
  float width = 0.0f;
  for (const FontPiece& piece : pieces) width = std::max(width, piece.at.x + piece.width);
  return width;
}

void GameFonts::read_coverage(Face& face, const std::string& name) {
  if (!name.empty()) {
    face.coverage = font_coverage(face.bytes.data(), face.bytes.size());
    return;
  }
  const EmbeddedFile embedded = embedded_ui_font();
  face.coverage = font_coverage(embedded.data, static_cast<std::size_t>(embedded.size));
}
void GameFonts::read_face(Face& face, const std::string& name) {
  if (name.empty()) return;
  face.bytes = read_font(directory_, name);
  face.failed = face.bytes.empty();
  if (face.failed) log::warning("font '" + printable(name) + "' is missing or unreadable; using the built-in font");
}
GameFonts::Face& GameFonts::face(const std::string& name) {
  const auto found = faces_.find(name);
  if (found != faces_.end()) return found->second;
  if (faces_.size() >= kMaxFontFaces) {
    if (!full_) log::warning("UI font cache is full; using the built-in font");
    full_ = true;
    return face("");
  }
  Face& added = faces_[name];
  for (int code = kFirstLetter; code <= kLastInitialLetter; ++code) added.letters.insert(code);
  read_face(added, name);
  read_coverage(added, name);
  return added;
}
bool GameFonts::add_letter(Face& face, int codepoint) {
  if (codepoint < kFirstLetter) return false;
  if (face.letters.count(codepoint) != 0) return false;
  if (face.letters.size() >= kMaxFontLetters) {
    if (!face.full) log::warning("UI font glyph limit reached; unsupported letters use the font's fallback");
    face.full = true;
    return false;
  }
  return face.letters.insert(codepoint).second;
}
bool GameFonts::add_letters(Face& face, const std::string& text) {
  bool changed = false;
  const char* at = text.c_str();
  while (*at != '\0') {
    int bytes = 0;
    const int codepoint = GetCodepointNext(at, &bytes);
    if (bytes <= 0) break;
    changed = add_letter(face, codepoint) || changed;
    at += bytes;
  }
  return changed;
}
void GameFonts::release_face(Face& face) {
  if (face.owned) {
    rlDrawRenderBatchActive();  // earlier HUD text may still refer to this atlas
    UnloadFont(face.font);
  }
  face.font = {};
  face.owned = false;
}
void GameFonts::rebuild(Face& face) {
  Font font = rasterize_face(face.bytes, face.letters);
  if (usable_font(font)) prepare_atlas(font);
  release_face(face);
  face.font = font;
  face.owned = owns_font(font);
  face.failed = !usable_font(font) || !visible_fallback(font);
  if (face.failed) {
    release_face(face);
    face.font = GetFontDefault();
    face.coverage.clear();  // its letters go to the next face of a chain from now on
  }
}
const Font& GameFonts::get(const std::string& name, const std::string& text) {
  if (faces_.empty()) face("");
  Face& selected = face(name);
  if (selected.failed) return name.empty() ? selected.font : get("", text);
  const bool changed = add_letters(selected, text);
  if (changed || selected.font.texture.id == 0) rebuild(selected);
  if (selected.failed && !name.empty()) {
    log::warning("font '" + printable(name) + "' could not be decoded; using the built-in font");
    return get("", text);
  }
  return selected.font;
}
bool GameFonts::covers(const std::string& name, int codepoint) {
  const Face& found = face(name);
  return std::binary_search(found.coverage.begin(), found.coverage.end(), codepoint);
}
std::vector<FontPiece> GameFonts::pieces(const std::vector<std::string>& names, const std::vector<FaceRun>& runs) {
  std::vector<std::string> letters(names.size());
  for (const FaceRun& run : runs) letters[run.face] += run.text;
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (!letters[i].empty()) get(names[i], letters[i]);  // each atlas grows once for the whole text
  }
  std::vector<FontPiece> laid;
  for (const FaceRun& run : runs)
    laid.push_back({is_line_break(run.text) ? nullptr : &get(names[run.face], run.text), run.text});
  return laid;
}
std::vector<FontPiece> GameFonts::layout(const std::string& chain, const std::string& text, float size, float spacing) {
  if (faces_.empty()) face("");
  const std::vector<std::string> names = chain_faces(chain);
  const auto has_glyph = [this, &names](std::size_t index, int codepoint) { return covers(names[index], codepoint); };
  std::vector<FontPiece> laid = pieces(names, face_runs(visual_order(text), names.size(), has_glyph));
  place_pieces(laid, size, spacing);
  return laid;
}
void GameFonts::set_directory(const std::filesystem::path& directory) {
  if (directory == directory_) return;
  clear();
  directory_ = directory;
  full_ = false;
}
void GameFonts::clear() {
  for (auto& [name, face] : faces_) release_face(face);
  faces_.clear();
  full_ = false;
}

}  // namespace redgem
