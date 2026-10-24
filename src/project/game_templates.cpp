#include "project/game_templates.hpp"

#include <map>
#include <string_view>

#include <nlohmann/json.hpp>

#include "app/game_settings.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "generated/templates.hpp"

namespace redgem {
namespace {

// What a template says about itself: a file of its folder that stays out of the games made from it.
constexpr const char* kTemplateInfoFile = "template.json";
constexpr const char* kDescriptionKey = "description";
constexpr const char* kIconKey = "icon";
constexpr char kPathSeparator = '/';  // of the paths files are built in under

// A file built into the program, as a text of its bytes.
std::string contents_of(const EmbeddedFile& file) {
  return {reinterpret_cast<const char*>(file.data), static_cast<std::size_t>(file.size)};
}

// The files of each template, by where they go in a game made from it.
using TemplateFiles = std::map<std::string, std::map<std::string, std::string>>;

const TemplateFiles& files_by_template() {
  static const TemplateFiles files = [] {
    TemplateFiles sorted;
    for (const auto& entry : generated::template_files()) {
      const std::string_view path = entry.path;
      const std::size_t slash = path.find(kPathSeparator);
      sorted[std::string(path.substr(0, slash))][std::string(path.substr(slash + 1))] = contents_of(entry.file);
    }
    return sorted;
  }();
  return files;
}

// What a template's own file says under `key`; nothing when it does not say it.
std::string info(const std::map<std::string, std::string>& files, const char* key) {
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

bool write_template_files(const std::filesystem::path& game_dir, const std::map<std::string, std::string>& files,
                          std::string& error) {
  for (const auto& [path, contents] : files) {
    if (path == kTemplateInfoFile) continue;
    const std::filesystem::path file = game_dir / path;
    std::filesystem::create_directories(file.parent_path());
    if (!write_text_file(file, contents, error)) return false;
  }
  return true;
}

// The game says what it is called and what it is known by, in its game.json.
bool name_game(const std::filesystem::path& game_dir, std::string& error) {
  GameSettings settings = read_game_settings(game_dir);
  settings.title = normalized_folder(game_dir).filename().string();
  settings.id = new_game_id(settings.title);
  return write_game_settings(game_dir, settings, error);
}

}  // namespace

const std::vector<GameTemplate>& game_templates() {
  static const std::vector<GameTemplate> templates = [] {
    std::vector<GameTemplate> found;
    for (const auto& [name, files] : files_by_template()) {
      found.push_back({name, info(files, kDescriptionKey), info(files, kIconKey)});
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
  const auto files = files_by_template().find(template_name);
  if (files == files_by_template().end()) {
    error = "unknown template '" + template_name + "': the templates are " + template_names(", ");
    return false;
  }
  if (!is_empty_dir(game_dir)) {
    error = game_dir.string() + " already exists and is not empty";
    return false;
  }
  create_game_folders(game_dir);
  return write_template_files(game_dir, files->second, error) && name_game(game_dir, error);
}

}  // namespace redgem
