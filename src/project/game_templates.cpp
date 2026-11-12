#include "project/game_templates.hpp"

#include <map>
#include <string_view>

#include <nlohmann/json.hpp>

#include "app/game_settings.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "generated/templates.hpp"
#include "project/api_rbs.hpp"
#include "project/assistant_guide.hpp"

namespace redgem {
namespace {

// What a template says about itself: a file of its folder that stays out of the games made from it.
constexpr const char* kTemplateInfoFile = "template.json";
constexpr const char* kDescriptionKey = "description";
constexpr const char* kIconKey = "icon";
constexpr char kPathSeparator = '/';  // of the paths files are built in under
// The folder of templates/ whose files every new game gets, whatever its template (it has no
// template.json: it is no template of its own).
constexpr const char* kSharedFolder = "_shared";

using FilesByPath = std::map<std::string, std::string>;

// A file built into the program, as a text of its bytes.
std::string contents_of(const EmbeddedFile& file) {
  return {reinterpret_cast<const char*>(file.data), static_cast<std::size_t>(file.size)};
}

// The files of each folder of templates/ (each template, and the shared files), by where they go
// in a game made from it.
using FolderFiles = std::map<std::string, FilesByPath>;

const FolderFiles& files_by_folder() {
  static const FolderFiles files = [] {
    FolderFiles sorted;
    for (const auto& entry : generated::template_files()) {
      const std::string_view path = entry.path;
      const std::size_t slash = path.find(kPathSeparator);
      sorted[std::string(path.substr(0, slash))][std::string(path.substr(slash + 1))] = contents_of(entry.file);
    }
    return sorted;
  }();
  return files;
}

// Whether a folder of templates/ is a template: one that says what it is.
bool is_template(const FilesByPath& files) { return files.count(kTemplateInfoFile) > 0; }

// The files of the template of that name; nothing when there is no such template.
const FilesByPath* files_of_template(const std::string& name) {
  const auto found = files_by_folder().find(name);
  if (found == files_by_folder().end()) return nullptr;
  return is_template(found->second) ? &found->second : nullptr;
}

// What every new game gets, whatever its template.
const FilesByPath& shared_files() {
  static const FilesByPath none;
  const auto found = files_by_folder().find(kSharedFolder);
  return found == files_by_folder().end() ? none : found->second;
}

// What a template's own file says under `key`; nothing when it does not say it.
std::string info(const FilesByPath& files, const char* key) {
  const auto file = files.find(kTemplateInfoFile);
  if (file == files.end()) return {};
  const nlohmann::json json = nlohmann::json::parse(file->second, nullptr, false);
  return json.is_object() ? json.value(key, std::string()) : std::string();
}

bool is_empty_dir(const std::filesystem::path& dir) {
  return !std::filesystem::exists(dir) || (std::filesystem::is_directory(dir) && std::filesystem::is_empty(dir));
}

void create_game_folders(const std::filesystem::path& game_dir) {
  for (const char* folder : {config::kScriptsDirName, config::kSoundsDirName, config::kScenesDirName,
                             config::kModelsDirName, config::kImagesDirName, config::kFontsDirName}) {
    std::filesystem::create_directories(game_dir / folder);
  }
}

bool write_template_files(const std::filesystem::path& game_dir, const FilesByPath& files, std::string& error) {
  for (const auto& [path, contents] : files) {
    if (path == kTemplateInfoFile) continue;
    const std::filesystem::path file = game_dir / path;
    std::filesystem::create_directories(file.parent_path());
    if (!write_text_file(file, contents, error)) return false;
  }
  return true;
}

// The game says what it is called, who makes it and what it is known by, in its game.json.
bool name_game(const std::filesystem::path& game_dir, std::string& error) {
  GameSettings settings = read_game_settings(game_dir);
  settings.title = normalized_folder(game_dir).filename().string();
  if (settings.studio.empty()) settings.studio = kNewGameStudio;
  settings.id = new_game_id(settings.title);
  return write_game_settings(game_dir, settings, error);
}

}  // namespace

const std::vector<GameTemplate>& game_templates() {
  static const std::vector<GameTemplate> templates = [] {
    std::vector<GameTemplate> found;
    for (const auto& [name, files] : files_by_folder()) {
      if (is_template(files)) found.push_back({name, info(files, kDescriptionKey), info(files, kIconKey)});
    }
    return found;
  }();
  return templates;
}

std::string template_names(const std::string& separator) {
  std::string names;
  for (const GameTemplate& known : game_templates()) names += (names.empty() ? "" : separator) + known.name;
  return names;
}

bool create_game(const std::filesystem::path& game_dir, const std::string& template_name, std::string& error) {
  const FilesByPath* files = files_of_template(template_name);
  if (files == nullptr) {
    error = "unknown template '" + template_name + "': the templates are " + template_names(", ");
    return false;
  }
  if (!is_empty_dir(game_dir)) {
    error = game_dir.string() + " already exists and is not empty";
    return false;
  }
  create_game_folders(game_dir);
<<<<<<< HEAD
  return write_template_files(game_dir, shared_files(), error) && write_template_files(game_dir, *files, error) &&
         name_game(game_dir, error) && tools::write_assistant_guide(game_dir, error);
=======
  return write_template_files(game_dir, files->second, error) && name_game(game_dir, error) &&
         tools::write_assistant_guide(game_dir, error) && tools::write_type_signatures(game_dir, error);
>>>>>>> 48fcd5aa (Type every parameter and return of the scripting API, and write RBS signatures)
}

}  // namespace redgem
