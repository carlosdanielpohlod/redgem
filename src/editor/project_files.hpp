#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace redgem {

// What a game folder holds, as the editor lists it (each list sorted by name):
//   scripts  scripts/*.rb             file names  ("player.rb")
//   models   models/*.glb             file names  ("tree.glb")
//   sounds   sounds/*.wav|ogg|mp3|flac  sound names ("jump", what Audio.play takes)
//   scenes   scene.glb, scenes/*.glb  scene names ("main" first, then "level2": what Game.load_scene takes)
struct ProjectFiles {
  std::vector<std::string> scripts;
  std::vector<std::string> models;
  std::vector<std::string> sounds;
  std::vector<std::string> scenes;

  static ProjectFiles scan(const std::filesystem::path& game_dir);
  bool operator==(const ProjectFiles& other) const;
};

}  // namespace redgem
