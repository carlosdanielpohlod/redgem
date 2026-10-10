#include "editor/project_files.hpp"

#include <algorithm>

#include "audio/audio.hpp"
#include "core/config.hpp"

namespace redgem {
namespace {

constexpr const char* kModelExtension = ".glb";

// Regular files of a folder with the extension (or any supported sound when `extension` is null).
std::vector<std::filesystem::path> files_in(const std::filesystem::path& dir, const char* extension) {
  std::vector<std::filesystem::path> found;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(dir, error)) {
    if (!entry.is_regular_file(error)) continue;
    if (extension == nullptr ? find_sound_file(dir, entry.path().stem().string()).has_value()
                             : entry.path().extension() == extension) {
      found.push_back(entry.path());
    }
  }
  return found;
}

std::vector<std::string> sorted_unique(std::vector<std::string> names) {
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  return names;
}

}  // namespace

ProjectFiles ProjectFiles::scan(const std::filesystem::path& game_dir) {
  ProjectFiles files;
  for (const auto& path : files_in(game_dir / config::kScriptsDirName, config::kRubyFileExtension)) {
    files.scripts.push_back(path.filename().string());
  }
  for (const auto& path : files_in(game_dir / config::kModelsDirName, kModelExtension)) {
    files.models.push_back(path.filename().string());
  }
  for (const auto& path : files_in(game_dir / config::kSoundsDirName, nullptr)) {
    files.sounds.push_back(path.stem().string());
  }
  for (const auto& path : files_in(game_dir / config::kScenesDirName, kModelExtension)) {
    files.scenes.push_back(path.stem().string());
  }
  files.scripts = sorted_unique(files.scripts);
  files.models = sorted_unique(files.models);
  files.sounds = sorted_unique(files.sounds);
  files.scenes = sorted_unique(files.scenes);
  return files;
}

bool ProjectFiles::operator==(const ProjectFiles& other) const {
  return scripts == other.scripts && models == other.models && sounds == other.sounds && scenes == other.scenes;
}

}  // namespace redgem
