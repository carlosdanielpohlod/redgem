#include "editor/project_files.hpp"

#include <algorithm>

#include "audio/audio.hpp"
#include "core/config.hpp"

namespace redgem {
namespace {

constexpr const char* kModelExtension = ".glb";
constexpr const char* kAnySound = nullptr;  // for files_in: every kind of sound file the engine plays

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

// The files of a folder as the editor lists them: with their extension (scripts, models) ...
std::vector<std::string> file_names_in(const std::filesystem::path& dir, const char* extension) {
  std::vector<std::string> names;
  for (const auto& path : files_in(dir, extension)) names.push_back(path.filename().string());
  return sorted_unique(std::move(names));
}

// ... or without it (sounds and scenes, which scripts ask for by name).
std::vector<std::string> names_in(const std::filesystem::path& dir, const char* extension) {
  std::vector<std::string> names;
  for (const auto& path : files_in(dir, extension)) names.push_back(path.stem().string());
  return sorted_unique(std::move(names));
}

}  // namespace

ProjectFiles ProjectFiles::scan(const std::filesystem::path& game_dir) {
  ProjectFiles files;
  files.scripts = file_names_in(game_dir / config::kScriptsDirName, config::kRubyFileExtension);
  files.models = file_names_in(game_dir / config::kModelsDirName, kModelExtension);
  files.sounds = names_in(game_dir / config::kSoundsDirName, kAnySound);
  files.scenes = names_in(game_dir / config::kScenesDirName, kModelExtension);
  return files;
}

bool ProjectFiles::operator==(const ProjectFiles& other) const {
  return scripts == other.scripts && models == other.models && sounds == other.sounds && scenes == other.scenes;
}

}  // namespace redgem
