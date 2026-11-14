#pragma once

#include <raylib.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
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
class GameFonts {
 public:
  ~GameFonts() { clear(); }
  GameFonts() = default;
  GameFonts(const GameFonts&) = delete;
  GameFonts& operator=(const GameFonts&) = delete;
  const Font& get(const std::string& name, const std::string& text);
  // The text laid out with a font chain (render/font_chain.hpp): in the order its letters are
  // drawn (render/bidi_text.hpp), each piece with the first face that has its letters.
  std::vector<FontPiece> layout(const std::string& chain, const std::string& text, float size, float spacing);
  // Whether a face has a glyph for a letter (the built-in face for an empty name).
  bool covers(const std::string& name, int codepoint);
  void set_directory(const std::filesystem::path& directory);
  void clear();

 private:
  struct Face {
    std::vector<std::uint8_t> bytes;
    std::set<int> letters;
    std::vector<int> coverage;  // the letters the face has, in increasing order
    Font font{};
    bool owned = false;
    bool failed = false;
    bool full = false;
  };
  Face& face(const std::string& name);
  void read_face(Face& face, const std::string& name);
  void read_coverage(Face& face, const std::string& name);
  std::vector<FontPiece> pieces(const std::vector<std::string>& names, const std::vector<FaceRun>& runs);
  bool add_letters(Face& face, const std::string& text);
  bool add_letter(Face& face, int codepoint);
  void rebuild(Face& face);
  void release_face(Face& face);
  std::filesystem::path directory_;
  std::map<std::string, Face> faces_;
  bool full_ = false;
};

}  // namespace redgem
