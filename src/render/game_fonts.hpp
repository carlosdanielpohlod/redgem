#pragma once

#include <raylib.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace redgem {

// Glyphs requested by UI text, with bounded font files and atlases. GPU resources outlive queued text.
// Each face has an atlas for each of a few raster sizes: a text is drawn from the smallest one at
// least as tall as it is (the largest, above all of them), so it is never stretched to its size.
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
  void set_directory(const std::filesystem::path& directory);
  void clear();

 private:
  struct File {
    std::vector<std::uint8_t> bytes;  // empty for the built-in face
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
  void read_file(File& file, const std::string& name) const;
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
