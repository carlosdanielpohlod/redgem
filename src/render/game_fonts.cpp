#include "render/game_fonts.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cstdlib>
#include <cstddef>
#include <iterator>
#include <utility>

#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "render/ui_font.hpp"
#include "render/font_rasterizer.hpp"

namespace redgem {
namespace {

// The raster sizes a face's atlases are made at, each half as tall again as the last: a text is
// drawn from the smallest at least as tall as it is, never shrunk by more than that. (Past the
// largest, it is stretched: an atlas any taller would not fit every graphics card's textures.)
constexpr int kRasterSizes[] = {16, 24, 36, 54, GameFonts::kDefaultPixels, 108, 144};
constexpr int kFirstLetter = 32;
constexpr int kLastInitialLetter = 255;
constexpr std::size_t kMaxFontFaces = 5;     // built-in face and four game fonts (each at any raster size)
constexpr std::size_t kMaxFontAtlases = 16;  // past it, a face is drawn from the nearest atlas it has
constexpr std::size_t kMaxFontLetters = 2048;
constexpr std::size_t kMaxFontBytes = 32u * 1024u * 1024u;
constexpr const char* kFontExtensions[] = {"", ".ttf", ".otf"};
constexpr const char* kFontType = ".ttf";  // raylib reads both TrueType and OpenType through this decoder
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
Font rasterize_face(const std::vector<std::uint8_t>& bytes, const std::set<int>& codes, int pixels) {
  if (!bytes.empty()) return rasterize_game_font(bytes, codes, pixels);
  const EmbeddedFile embedded = embedded_ui_font();
  std::vector<int> letters(codes.begin(), codes.end());
  return LoadFontFromMemory(kFontType, embedded.data, embedded.size, pixels, letters.data(),
                            static_cast<int>(letters.size()));
}

// Whether a raster size suits text `pixels` tall better than the best one found yet (any does,
// before there is one): one at least as tall beats one shorter (which would stretch its letters);
// then the nearer.
bool nearer(int raster, const int* best, int pixels) {
  if (best == nullptr) return true;
  const bool tall_enough = raster >= pixels;
  if (tall_enough != (*best >= pixels)) return tall_enough;
  return std::abs(raster - pixels) < std::abs(*best - pixels);
}

}  // namespace

int GameFonts::raster_pixels(int pixels) {
  for (const int size : kRasterSizes) {
    if (size >= pixels) return size;
  }
  return std::end(kRasterSizes)[-1];
}
void GameFonts::read_file(File& file, const std::string& name) const {
  if (name.empty()) return;
  file.bytes = read_font(directory_, name);
  file.failed = file.bytes.empty();
  if (file.failed) log::warning("font '" + printable(name) + "' is missing or unreadable; using the built-in font");
}
GameFonts::File& GameFonts::file(const std::string& name) {
  const auto found = files_.find(name);
  if (found != files_.end()) return found->second;
  File& added = files_[name];
  read_file(added, name);
  return added;
}
GameFonts::Face& GameFonts::add_face(const std::string& name, int pixels) {
  Face& added = faces_[{name, pixels}];
  added.file = &file(name);
  added.pixels = pixels;
  added.failed = added.file->failed;
  for (int code = kFirstLetter; code <= kLastInitialLetter; ++code) added.letters.insert(code);
  return added;
}
bool GameFonts::no_room_for_file(const std::string& name) const {
  return files_.count(name) == 0 && files_.size() >= kMaxFontFaces;
}
// The atlas of the face `name` whose raster size is nearest `pixels`; null when it has none.
GameFonts::Face* GameFonts::nearest_atlas(const std::string& name, int pixels) {
  Face* nearest = nullptr;
  for (auto& [key, kept] : faces_) {
    if (key.first != name) continue;
    if (nearer(key.second, nearest ? &nearest->pixels : nullptr, pixels)) nearest = &kept;
  }
  return nearest;
}
// With every atlas taken: the face's own that suits the size best, or the built-in face's (a new
// one only when the built-in face has none at all).
GameFonts::Face& GameFonts::kept_atlas(const std::string& name, int pixels) {
  Face* kept = nearest_atlas(name, pixels);
  if (kept == nullptr) kept = nearest_atlas("", pixels);
  return kept ? *kept : add_face("", pixels);
}
GameFonts::Face& GameFonts::face(const std::string& name, int pixels) {
  if (files_.empty()) file("");  // the built-in face is always the first kept
  const auto found = faces_.find({name, pixels});
  if (found != faces_.end()) return found->second;
  if (no_room_for_file(name)) {
    if (!full_) log::warning("UI font cache is full; using the built-in font");
    full_ = true;
    return face("", pixels);
  }
  return faces_.size() >= kMaxFontAtlases ? kept_atlas(name, pixels) : add_face(name, pixels);
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
  Font font = rasterize_face(face.file->bytes, face.letters, face.pixels);
  if (usable_font(font)) prepare_atlas(font);
  release_face(face);
  face.font = font;
  face.owned = owns_font(font);
  face.failed = !usable_font(font) || !visible_fallback(font);
  if (face.failed) {
    release_face(face);
    face.font = GetFontDefault();
    return;
  }
}
const Font& GameFonts::fallback(const std::string& name, const std::string& text, int pixels) {
  return name.empty() ? face(name, raster_pixels(pixels)).font : get("", text, pixels);
}
const Font& GameFonts::get(const std::string& name, const std::string& text, int pixels) {
  Face& selected = face(name, raster_pixels(pixels));
  if (selected.failed) return fallback(name, text, pixels);
  const bool changed = add_letters(selected, text);
  if (changed || selected.font.texture.id == 0) rebuild(selected);
  if (selected.failed && !name.empty()) {
    log::warning("font '" + printable(name) + "' could not be decoded; using the built-in font");
    return get("", text, pixels);
  }
  return selected.font;
}
void GameFonts::set_directory(const std::filesystem::path& directory) {
  if (directory == directory_) return;
  clear();
  directory_ = directory;
  full_ = false;
}
void GameFonts::clear() {
  for (auto& [key, face] : faces_) release_face(face);
  faces_.clear();
  files_.clear();
  full_ = false;
}

}  // namespace redgem
