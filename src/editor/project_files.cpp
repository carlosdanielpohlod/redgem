#include "editor/project_files.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>

#include "audio/audio.hpp"
#include "core/config.hpp"

namespace redgem {
namespace {

constexpr const char* kModelExtension = ".glb";
constexpr const char* kScriptExtension = config::kRubyFileExtension;

// What a folder lists: files with one extension, or every kind of sound file the engine plays,
// or every kind of picture a material shows.
enum class Listing { Extension, Sounds, Pictures };
struct FileKind {
  Listing listing = Listing::Extension;
  const char* extension = nullptr;  // for Listing::Extension
};
constexpr FileKind kModels{Listing::Extension, kModelExtension};
constexpr FileKind kScripts{Listing::Extension, kScriptExtension};
constexpr FileKind kSounds{Listing::Sounds};
constexpr FileKind kPictures{Listing::Pictures};

std::string lowered(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
  return text;
}

// A regular file of a folder, not a link (built games leave links out).
bool plain_file(const std::filesystem::directory_entry& entry) {
  std::error_code error;
  return !entry.is_symlink(error) && entry.is_regular_file(error);
}

// Whether a file of `dir` is of the kind listed.
bool of_kind(const std::filesystem::path& dir, const std::filesystem::path& file, const FileKind& kind) {
  if (kind.listing == Listing::Sounds) return find_sound_file(dir, file.stem().string()).has_value();
  if (kind.listing == Listing::Pictures) return is_picture_file(file);
  return file.extension() == kind.extension;
}

bool listed(const std::filesystem::path& dir, const std::filesystem::directory_entry& entry, const FileKind& kind) {
  return plain_file(entry) && of_kind(dir, entry.path(), kind);
}

// The plain files of a folder of one kind.
std::vector<std::filesystem::path> files_in(const std::filesystem::path& dir, const FileKind& kind) {
  std::vector<std::filesystem::path> found;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(dir, error)) {
    if (listed(dir, entry, kind)) found.push_back(entry.path());
  }
  return found;
}

std::vector<std::string> sorted_unique(std::vector<std::string> names) {
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  return names;
}

// The files of a folder as the editor lists them: with their extension (scripts, models) ...
std::vector<std::string> file_names_in(const std::filesystem::path& dir, const FileKind& kind) {
  std::vector<std::string> names;
  for (const auto& path : files_in(dir, kind)) names.push_back(path.filename().string());
  return sorted_unique(std::move(names));
}

// ... or without it (sounds and scenes, which scripts ask for by name).
std::vector<std::string> names_in(const std::filesystem::path& dir, const FileKind& kind) {
  std::vector<std::string> names;
  for (const auto& path : files_in(dir, kind)) names.push_back(path.stem().string());
  return sorted_unique(std::move(names));
}

// The game's scenes by the names scripts load them with: "main" (its scene.glb) first, then those
// of scenes/. A scenes/main.glb is left out: no script reaches it, "main" is scene.glb.
std::vector<std::string> scene_names(const std::filesystem::path& game_dir) {
  std::vector<std::string> names = names_in(game_dir / config::kScenesDirName, kModels);
  names.erase(std::remove(names.begin(), names.end(), config::kMainSceneName), names.end());
  std::error_code error;
  if (std::filesystem::exists(game_dir / config::kSceneFileName, error))
    names.insert(names.begin(), config::kMainSceneName);
  return names;
}

}  // namespace

bool is_picture_file(const std::filesystem::path& file) {
  const std::string extension = lowered(file.extension().string());
  return std::any_of(std::begin(kImageFileExtensions), std::end(kImageFileExtensions),
                     [&extension](const char* known) { return extension == known; });
}

ProjectFiles ProjectFiles::scan(const std::filesystem::path& game_dir) {
  ProjectFiles files;
  files.scripts = file_names_in(game_dir / config::kScriptsDirName, kScripts);
  files.models = file_names_in(game_dir / config::kModelsDirName, kModels);
  files.sounds = names_in(game_dir / config::kSoundsDirName, kSounds);
  files.scenes = scene_names(game_dir);
  files.images = file_names_in(game_dir / config::kImagesDirName, kPictures);
  return files;
}

bool ProjectFiles::operator==(const ProjectFiles& other) const {
  return scripts == other.scripts && models == other.models && sounds == other.sounds && scenes == other.scenes &&
         images == other.images;
}

}  // namespace redgem
