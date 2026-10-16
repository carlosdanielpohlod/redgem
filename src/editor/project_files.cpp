#include "editor/project_files.hpp"

#include <algorithm>

#include "audio/audio.hpp"
#include "core/config.hpp"

namespace redgem {
namespace {

constexpr const char* kModelExtension = ".glb";
constexpr const char* kAnySound = nullptr;  // for files_in: every kind of sound file the engine plays

// A regular file of a folder, not a link (built games leave links out).
bool plain_file(const std::filesystem::directory_entry& entry) {
  std::error_code error;
  return !entry.is_symlink(error) && entry.is_regular_file(error);
}

// Whether a file of `dir` is of the kind listed: with the extension, or any supported sound when
// `extension` is null.
bool of_kind(const std::filesystem::path& dir, const std::filesystem::path& file, const char* extension) {
  if (extension == nullptr) return find_sound_file(dir, file.stem().string()).has_value();
  return file.extension() == extension;
}

bool listed(const std::filesystem::path& dir, const std::filesystem::directory_entry& entry, const char* extension) {
  return plain_file(entry) && of_kind(dir, entry.path(), extension);
}

// The plain files of a folder of one kind.
std::vector<std::filesystem::path> files_in(const std::filesystem::path& dir, const char* extension) {
  std::vector<std::filesystem::path> found;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(dir, error)) {
    if (listed(dir, entry, extension)) found.push_back(entry.path());
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
