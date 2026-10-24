#include "render/font_rasterizer.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstddef>
#include <cstring>

namespace redgem {
namespace {

constexpr int kMetricUnit = 64;  // FreeType metrics use 26.6 fixed-point pixels
constexpr unsigned kLargestGlyphSide = 512;
constexpr std::size_t kGlyphPixelBudget = 16u * 1024u * 1024u;
constexpr int kFontAtlasPadding = 4;
constexpr int kSkylinePacking = 1;  // packs actual glyph heights, including tall accents
constexpr std::size_t kPaddingEdges = 2;
constexpr int kLargestAtlasSide = 8192;
constexpr FT_Int32 kGlyphFlags = FT_LOAD_RENDER | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP;

bool request_pixel_height(FT_Face face, int pixels) {
  FT_Size_RequestRec request{};
  request.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;  // match the built-in font's ascent-to-descent pixel height
  request.height = static_cast<FT_Long>(pixels) * kMetricUnit;
  return FT_Request_Size(face, &request) == 0;
}

class FontFace {
 public:
  FontFace(const std::vector<std::uint8_t>& bytes, int pixels) {
    if (FT_Init_FreeType(&library_) != 0) return;
    if (FT_New_Memory_Face(library_, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &face) != 0) return;
    if (!FT_IS_SFNT(face)) return;
    ready = request_pixel_height(face, pixels);
  }
  ~FontFace() {
    if (face != nullptr) FT_Done_Face(face);
    if (library_ != nullptr) FT_Done_FreeType(library_);
  }
  FontFace(const FontFace&) = delete;
  FontFace& operator=(const FontFace&) = delete;
  FT_Face face = nullptr;
  bool ready = false;

 private:
  FT_Library library_ = nullptr;
};

bool bounded_bitmap(const FT_Bitmap& bitmap) {
  return bitmap.width <= kLargestGlyphSide && bitmap.rows <= kLargestGlyphSide;
}
bool grayscale_bitmap(const FT_Bitmap& bitmap) {
  return bitmap.width == 0 || bitmap.rows == 0 || bitmap.pixel_mode == FT_PIXEL_MODE_GRAY;
}
void copy_bitmap_rows(unsigned char* data, const FT_Bitmap& bitmap) {
  for (unsigned row = 0; row < bitmap.rows; ++row) {
    std::memcpy(data + static_cast<std::size_t>(row) * bitmap.width,
                bitmap.buffer + static_cast<std::ptrdiff_t>(row) * bitmap.pitch, bitmap.width);
  }
}
bool glyph_pixels_fit(std::size_t size, std::size_t remaining) { return size != 0 && size <= remaining; }
Image glyph_image(const FT_Bitmap& bitmap, std::size_t& remaining) {
  if (!bounded_bitmap(bitmap)) return {};
  if (!grayscale_bitmap(bitmap)) return {};
  const std::size_t size = static_cast<std::size_t>(bitmap.width) * bitmap.rows;
  if (!glyph_pixels_fit(size, remaining)) return {};
  auto* data = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(size)));
  if (data == nullptr) return {};
  copy_bitmap_rows(data, bitmap);
  remaining -= size;
  return {data, static_cast<int>(bitmap.width), static_cast<int>(bitmap.rows), 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
}
bool rejected_bitmap(const FT_Bitmap& bitmap, const Image& image) {
  return bitmap.width != 0 && bitmap.rows != 0 && !IsImageValid(image);
}
void set_glyph_metrics(GlyphInfo& glyph, FT_Face face) {
  const auto slot = face->glyph;
  glyph.advanceX = static_cast<int>(slot->advance.x / kMetricUnit);
  glyph.offsetX = slot->bitmap_left;
  glyph.offsetY = static_cast<int>(face->size->metrics.ascender / kMetricUnit) - slot->bitmap_top;
}
GlyphInfo glyph_of(FT_Face face, int codepoint, std::size_t& remaining) {
  GlyphInfo glyph{};
  glyph.value = codepoint;
  if (FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint)) == 0) return glyph;
  if (FT_Load_Char(face, static_cast<FT_ULong>(codepoint), kGlyphFlags) != 0) return glyph;
  glyph.image = glyph_image(face->glyph->bitmap, remaining);
  if (!rejected_bitmap(face->glyph->bitmap, glyph.image)) set_glyph_metrics(glyph, face);
  return glyph;
}
bool bounded_atlas(const Image& image) {
  return IsImageValid(image) && image.width <= kLargestAtlasSide && image.height <= kLargestAtlasSide;
}
bool atlas_budget_fits(const Font& font) {
  std::size_t width = 0;
  for (int i = 0; i < font.glyphCount; ++i)
    width += static_cast<std::size_t>(font.glyphs[i].image.width) + kPaddingEdges * font.glyphPadding;
  const auto row_height = static_cast<std::size_t>(font.baseSize) + kPaddingEdges * font.glyphPadding;
  return width <= kGlyphPixelBudget / row_height;
}
bool packed_glyph(const Rectangle& rect, const Image& atlas) {
  return rect.x >= 0 && rect.y >= 0 && rect.x + rect.width <= atlas.width && rect.y + rect.height <= atlas.height;
}
void discard_unpacked(Font& font, const Image& atlas) {
  for (int i = 0; i < font.glyphCount; ++i) {
    if (packed_glyph(font.recs[i], atlas)) continue;
    font.recs[i] = {};
    font.glyphs[i].value = 0;  // resolve to the visible fallback, never sample outside the atlas
  }
}
void upload_atlas(Font& font) {
  if (!atlas_budget_fits(font)) return;
  Image atlas =
      GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, font.baseSize, font.glyphPadding, kSkylinePacking);
  if (bounded_atlas(atlas)) {
    discard_unpacked(font, atlas);
    font.texture = LoadTextureFromImage(atlas);
  }
  UnloadImage(atlas);
}
Font glyph_font(std::size_t count, int pixels) {
  Font font{};
  font.baseSize = pixels;
  font.glyphCount = static_cast<int>(count);
  font.glyphPadding = kFontAtlasPadding;
  font.glyphs = static_cast<GlyphInfo*>(MemAlloc(static_cast<unsigned int>(count * sizeof(GlyphInfo))));
  return font;
}
void fill_glyphs(Font& font, FT_Face face, const std::set<int>& letters) {
  std::size_t remaining = kGlyphPixelBudget;
  int index = 0;
  for (const int code : letters) font.glyphs[index++] = glyph_of(face, code, remaining);
}

}  // namespace

Font rasterize_game_font(const std::vector<std::uint8_t>& bytes, const std::set<int>& letters, int pixels) {
  FontFace decoder(bytes, pixels);
  if (!decoder.ready) return {};
  Font font = glyph_font(letters.size(), pixels);
  if (font.glyphs == nullptr) return {};
  fill_glyphs(font, decoder.face, letters);
  upload_atlas(font);
  if (font.texture.id == 0) {
    UnloadFont(font);
    return {};
  }
  return font;
}

}  // namespace redgem
