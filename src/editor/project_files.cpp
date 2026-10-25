#include "editor/project_files.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>

#include "audio/audio.hpp"
#include "core/config.hpp"

namespace redgem {
namespace {

constexpr const char* kModelExtension = ".glb";
constexpr const char* kAnySound = nullptr;  // for files_in: every kind of sound file the engine plays
constexpr const char* kAnyImage = "";       // for files_in: every kind of picture file a material shows

std::string lowered(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
  return text;
}

bool picture_file(const std::filesystem::path& file) {
  const std::string extension = lowered(file.extension().string());
  return std::any_of(std::begin(kImageFileExtensions), std::end(kImageFileExtensions),
                     [&extension](const char* known) { return extension == known; });
}

// A regular file of a folder, not a link (built games leave links out).
bool plain_file(const std::filesystem::directory_entry& entry) {
  std::error_code error;
  return !entry.is_symlink(error) && entry.is_regular_file(error);
}

// Whether a file of `dir` is of the kind listed: with the extension, any supported sound when
// `extension` is kAnySound, or any picture when it is kAnyImage.
bool of_kind(const std::filesystem::path& dir, const std::filesystem::path& file, const char* extension) {
  if (extension == kAnySound) return find_sound_file(dir, file.stem().string()).has_value();
  if (extension == kAnyImage) return picture_file(file);
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

// The game's scenes by the names scripts load them with: "main" (its scene.glb) first, then those
// of scenes/. A scenes/main.glb is left out: no script reaches it, "main" is scene.glb.
std::vector<std::string> scene_names(const std::filesystem::path& game_dir) {
  std::vector<std::string> names = names_in(game_dir / config::kScenesDirName, kModelExtension);
  names.erase(std::remove(names.begin(), names.end(), config::kMainSceneName), names.end());
  std::error_code error;
  if (std::filesystem::exists(game_dir / config::kSceneFileName, error))
    names.insert(names.begin(), config::kMainSceneName);
  return names;
}

}  // namespace

ProjectFiles ProjectFiles::scan(const std::filesystem::path& game_dir) {
  ProjectFiles files;
  files.scripts = file_names_in(game_dir / config::kScriptsDirName, config::kRubyFileExtension);
  files.models = file_names_in(game_dir / config::kModelsDirName, kModelExtension);
  files.sounds = names_in(game_dir / config::kSoundsDirName, kAnySound);
  files.scenes = scene_names(game_dir);
  files.images = file_names_in(game_dir / config::kImagesDirName, kAnyImage);
  return files;
}

bool ProjectFiles::operator==(const ProjectFiles& other) const {
  return scripts == other.scripts && models == other.models && sounds == other.sounds && scenes == other.scenes &&
         images == other.images;
}

}  // namespace redgem
