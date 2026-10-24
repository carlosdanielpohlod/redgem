#pragma once

#include <raylib.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace redgem {

// Glyphs requested by UI text, with bounded font files and atlases. GPU resources outlive queued text.
class GameFonts {
 public:
  ~GameFonts() { clear(); }
  GameFonts() = default;
  GameFonts(const GameFonts&) = delete;
  GameFonts& operator=(const GameFonts&) = delete;
  const Font& get(const std::string& name, const std::string& text);
  void set_directory(const std::filesystem::path& directory);
  void clear();

 private:
  struct Face {
    std::vector<std::uint8_t> bytes;
    std::set<int> letters;
    Font font{};
    bool owned = false;
    bool failed = false;
    bool full = false;
  };
  Face& face(const std::string& name);
  void read_face(Face& face, const std::string& name);
  bool add_letters(Face& face, const std::string& text);
  bool add_letter(Face& face, int codepoint);
  void rebuild(Face& face);
  void release_face(Face& face);
  std::filesystem::path directory_;
  std::map<std::string, Face> faces_;
  bool full_ = false;
};

}  // namespace redgem
