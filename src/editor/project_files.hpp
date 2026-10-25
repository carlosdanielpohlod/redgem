#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace redgem {

// The kinds of picture file a material can show (what the images/ folder lists, and what dropped
// on the editor goes there), lowercase.
inline constexpr const char* kImageFileExtensions[] = {".png", ".jpg", ".jpeg", ".webp"};

// Whether a file is a picture a material can show, by its extension (whatever its case).
bool is_picture_file(const std::filesystem::path& file);

// What a game folder holds, as the editor lists it (each list sorted by name):
//   scripts  scripts/*.rb             file names  ("player.rb")
//   models   models/*.glb             file names  ("tree.glb")
//   sounds   sounds/*.wav|ogg|mp3|flac  sound names ("jump", what Audio.play takes)
//   scenes   scene.glb, scenes/*.glb  scene names ("main" first, then "level2": what Game.load_scene takes)
//   images   images/*.png|jpg|jpeg|webp  file names ("wood.png"): pictures for materials
struct ProjectFiles {
  std::vector<std::string> scripts;
  std::vector<std::string> models;
  std::vector<std::string> sounds;
  std::vector<std::string> scenes;
  std::vector<std::string> images;

  static ProjectFiles scan(const std::filesystem::path& game_dir);
  bool operator==(const ProjectFiles& other) const;
};

}  // namespace redgem
