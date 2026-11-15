#pragma once

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace redgem {

struct FaceRun;

// A piece of a text drawn with one face, where it starts from the text's top-left corner, and how
// wide it is. A line break is a piece with no font.
struct FontPiece {
  const Font* font = nullptr;
  std::string text;
  Vector2 at{};
  float width = 0.0f;
};

// How wide laid-out text is: its widest line.
float laid_out_width(const std::vector<FontPiece>& pieces);

// Glyphs requested by UI text, with bounded font files and atlases. GPU resources outlive queued text.
// Each face has an atlas for each of a few raster sizes: a text is drawn from the smallest one at
// least as tall as it is (the largest, above all of them), so it is not stretched to its size.
class GameFonts {
 public:
  ~GameFonts() { clear(); }
  GameFonts() = default;
  GameFonts(const GameFonts&) = delete;
  GameFonts& operator=(const GameFonts&) = delete;
  static constexpr int kDefaultPixels = 72;  // the raster size of a face asked for at no size
  // The face `name` (empty: the built-in one) with every letter of `text`, made for text `pixels` tall.
  const Font& get(const std::string& name, const std::string& text, int pixels = kDefaultPixels);
  // The raster size a text `pixels` tall is drawn from.
  static int raster_pixels(int pixels);
  // The text laid out with a font chain (render/font_chain.hpp), `size` pixels tall: in the order
  // its letters are drawn (render/bidi_text.hpp), each piece with the first face that has its letters.
  std::vector<FontPiece> layout(const std::string& chain, const std::string& text, float size, float spacing);
  // Whether a face has a glyph for a letter and room for it (the built-in face for an empty name).
  bool covers(const std::string& name, int codepoint, int pixels = kDefaultPixels);
  void set_directory(const std::filesystem::path& directory);
  void clear();

 private:
  struct File {
    std::vector<std::uint8_t> bytes;  // empty for the built-in face
    std::vector<int> coverage;        // the letters the file has, in increasing order
    bool failed = false;
  };
  struct Face {
    const File* file = nullptr;
    int pixels = kDefaultPixels;
    std::set<int> letters;
    Font font{};
    bool owned = false;
    bool failed = false;
    bool full = false;
  };
  using FaceKey = std::pair<std::string, int>;  // a face's name, and the raster size of its atlas
  Face& face(const std::string& name, int pixels);
  Face& add_face(const std::string& name, int pixels);
  File& file(const std::string& name);
  bool no_room_for_file(const std::string& name) const;
  Face* nearest_atlas(const std::string& name, int pixels);
  Face& kept_atlas(const std::string& name, int pixels);
  void read_file(File& file, const std::string& name) const;
  static void read_coverage(File& file);
  static bool has_room_for(const Face& face, int codepoint);
  static bool can_draw(const Face& face, int codepoint);
  std::vector<FontPiece> pieces(const std::vector<std::string>& names, const std::vector<FaceRun>& runs,
                                const std::function<bool(std::size_t, int)>& has_glyph, int pixels);
  const Font& fallback(const std::string& name, const std::string& text, int pixels);
  bool add_letters(Face& face, const std::string& text);
  bool add_letter(Face& face, int codepoint);
  void rebuild(Face& face);
  void release_face(Face& face);
  std::filesystem::path directory_;
  std::map<std::string, File> files_;
  std::map<FaceKey, Face> faces_;
  bool full_ = false;
};

}  // namespace redgem
