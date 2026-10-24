#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "render/game_fonts.hpp"
#include "render/ui_font.hpp"
#include "render_system/render_stage.hpp"

using namespace redgem;
using redgem::test::RenderStage;

namespace {
constexpr const char* kUnicodeText = "€ Żółć ş Ω Ж";
constexpr int kEuro = 0x20ac;
constexpr int kPolish = 0x017b;
constexpr int kTurkish = 0x015f;
constexpr int kGreek = 0x03a9;
constexpr int kCyrillic = 0x0416;
constexpr int kUnsupported = 0x10ffff;
constexpr int kQuestion = 63;
constexpr std::size_t kEnoughPixels = 100;
constexpr unsigned char kDark = 100;
constexpr int kFirstLineRows = 50;

class FontFiles {
 public:
  const std::filesystem::path folder = std::filesystem::temp_directory_path() / "redgem-font-render";
  FontFiles() {
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    const EmbeddedFile inter = embedded_ui_font();
    std::ofstream file(folder / "custom.ttf", std::ios::binary);
    file.write(reinterpret_cast<const char*>(inter.data), inter.size);
    std::ofstream(folder / "broken.ttf", std::ios::binary) << "not a font";
  }
  ~FontFiles() { std::filesystem::remove_all(folder); }
};

HudItem text_item(const std::string& text, int down, const std::string& font = "") {
  HudItem item;
  item.text = text;
  item.font = font;
  item.x = 10;
  item.y = down;
  item.size = 28;
  item.color = {0, 0, 0, 1};
  return item;
}
bool dark(::Color color) { return color.r < kDark && color.g < kDark && color.b < kDark; }
}  // namespace

TEST_CASE("game fonts add non Latin-1 glyphs on demand and retain a visible fallback") {
  RenderStage stage;
  GameFonts fonts;
  const auto initial_count = fonts.get("", "ASCII").glyphCount;
  const Font& unicode = fonts.get("", kUnicodeText);
  CHECK(unicode.glyphCount > initial_count);
  for (const int code : {kEuro, kPolish, kTurkish, kGreek, kCyrillic}) {
    const auto glyph = GetGlyphInfo(unicode, code);
    CHECK(glyph.value == code);
    CHECK(glyph.image.width > 0);
  }
  const auto cached = unicode.texture.id;
  CHECK(fonts.get("", kUnicodeText).texture.id == cached);
  const Font& fallback = fonts.get("", "\xf4\x8f\xbf\xbf");
  CHECK(GetGlyphInfo(fallback, kUnsupported).value == kQuestion);
}

TEST_CASE("game-provided fonts load independently and missing or unsafe names use the built-in face") {
  RenderStage stage;
  FontFiles files;
  GameFonts fonts;
  fonts.set_directory(files.folder);
  const auto builtin = fonts.get("", "test").texture.id;
  const auto custom = fonts.get("custom", "test").texture.id;
  REQUIRE(custom != 0);
  CHECK(custom != builtin);
  CHECK(fonts.get("custom", "test").texture.id == custom);
  CHECK(fonts.get("broken", "test").texture.id == builtin);
  CHECK(fonts.get("absent", "test").texture.id == builtin);
  CHECK(fonts.get("../outside", "test").texture.id == builtin);
  fonts.set_directory(files.folder / "new_game");
  const auto next_builtin = fonts.get("", "test").texture.id;
  CHECK(fonts.get("custom", "test").texture.id == next_builtin);
}

TEST_CASE("Unicode text and a game-provided font draw into the HUD together") {
  RenderStage stage;
  FontFiles files;
  std::filesystem::create_directories(files.folder / "images");
  std::filesystem::create_directories(files.folder / "fonts");
  std::filesystem::copy_file(files.folder / "custom.ttf", files.folder / "fonts/custom.ttf");
  stage.set_images_dir(files.folder / "images");
  const auto picture =
      stage.draw_hud({text_item(kUnicodeText, 10), text_item(kUnicodeText, 60, "custom")}, "unicode_game_fonts");
  CHECK(picture.find(dark).count > kEnoughPixels);
  CHECK(picture.at(0.05f, 0.8f).r > kDark);  // untouched background remains below the text
}

TEST_CASE("queued text remains visible when a later line grows the font atlas") {
  RenderStage stage;
  const auto picture = stage.draw_hud({text_item("ASCII", 10), text_item(kUnicodeText, 60)}, "unicode_atlas_growth");
  CHECK(picture.find(dark).count > kEnoughPixels);
  const auto end = picture.pixels.begin() + picture.width * kFirstLineRows;
  CHECK(std::count_if(picture.pixels.begin(), end, dark) > kEnoughPixels);  // earlier text survived atlas release
}

TEST_CASE("font glyph growth is bounded and changing games resets the glyph budget") {
  constexpr int kRequestedLetters = 4096;
  constexpr int kFirstRequested = 0x0100;
  constexpr int kLargestAtlas = 2048;
  RenderStage stage;
  GameFonts fonts;
  std::string many;
  for (int i = 0; i < kRequestedLetters; ++i) {
    int bytes = 0;
    const char* encoded = CodepointToUTF8(kFirstRequested + i, &bytes);
    many.append(encoded, static_cast<std::size_t>(bytes));
  }
  const Font& bounded = fonts.get("", many);
  CHECK(bounded.texture.id != 0);
  CHECK(bounded.glyphCount <= kLargestAtlas);
  const auto saturated = bounded.texture.id;
  CHECK(fonts.get("", "€").texture.id == saturated);
  fonts.set_directory(std::filesystem::temp_directory_path() / "redgem-next-font-game");
  CHECK(GetGlyphInfo(fonts.get("", "€"), kEuro).value == kEuro);
}

TEST_CASE("named font cache is bounded and refuses oversized or invalid table directories") {
  constexpr int kNamedFaces = 4;
  constexpr std::uintmax_t kOversizedBytes = 33u * 1024u * 1024u;
  RenderStage stage;
  FontFiles files;
  GameFonts fonts;
  fonts.set_directory(files.folder);
  const auto builtin = fonts.get("", "text").texture.id;
  std::ofstream(files.folder / "large.ttf", std::ios::binary);
  std::filesystem::resize_file(files.folder / "large.ttf", kOversizedBytes);
  CHECK(fonts.get("large", "text").texture.id == builtin);
  fonts.clear();
  const std::vector<unsigned char> invalid_tables{0,   1,   0, 0, 0, 1, 0,   0,   0,   0,   0, 0, 'c', 'm',
                                                  'a', 'p', 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0,   8};
  std::ofstream malformed(files.folder / "tables.ttf", std::ios::binary);
  malformed.write(reinterpret_cast<const char*>(invalid_tables.data()),
                  static_cast<std::streamsize>(invalid_tables.size()));
  malformed.close();
  const auto fallback = fonts.get("", "text").texture.id;
  CHECK(fonts.get("tables", "text").texture.id == fallback);
  fonts.clear();
  const auto base = fonts.get("", "text").texture.id;
  for (int i = 0; i <= kNamedFaces; ++i) {
    const std::string name = "face" + std::to_string(i);
    std::filesystem::copy_file(files.folder / "custom.ttf", files.folder / (name + ".ttf"));
    const auto selected = fonts.get(name, "text").texture.id;
    if (i < kNamedFaces)
      CHECK(selected != base);
    else
      CHECK(selected == base);
  }
}

TEST_CASE("selecting the same font from disk retains the built-in text scale") {
  constexpr float kTextPixels = 28.0f;
  constexpr float kRoundingTolerance = 0.02f;
  RenderStage stage;
  FontFiles files;
  GameFonts fonts;
  fonts.set_directory(files.folder);
  const auto builtin = MeasureTextEx(fonts.get("", "Sample"), "Sample", kTextPixels, 0).x;
  const auto custom = MeasureTextEx(fonts.get("custom", "Sample"), "Sample", kTextPixels, 0).x;
  CHECK(custom == doctest::Approx(builtin).epsilon(kRoundingTolerance));
}
