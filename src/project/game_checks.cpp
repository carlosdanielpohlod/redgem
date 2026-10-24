#include "project/game_checks.hpp"

#include <algorithm>
#include <optional>
#include <cctype>
#include <cstring>
#include <sstream>
#include <set>

#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"
#include "project/closest_words.hpp"
#include "project/scene_command.hpp"
#include "project/scene_text.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/props.hpp"
#include "script/ruby_vm.hpp"
#include "script/script_naming.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;

// How many names a message that suggests some lists.
constexpr std::size_t kMostSuggestions = 3;
// What a sky picture may be named, after its name (as render/sky_picture.cpp looks for it).
constexpr const char* kSkyExtensions[] = {"", ".hdr", ".png", ".jpg", ".jpeg"};
// What a syntax error starts with, and what goes between a file and its line instead.
constexpr const char* kLineWord = "line ";
constexpr const char* kLineJoiner = ":";
// How a class is defined: "class", at least one blank, its name.
constexpr const char* kClassWord = "class ";
constexpr const char* kBlanks = " \t";

// The game's scripts, as paths inside its scripts folder ("enemies/slime.rb"), sorted.
std::vector<std::string> script_files(const fs::path& scripts_dir) {
  std::vector<std::string> files;
  std::error_code error;
  for (fs::recursive_directory_iterator entry(scripts_dir, error), end; !error && entry != end;
       entry.increment(error)) {
    if (entry->path().extension() == config::kRubyFileExtension) {
      files.push_back(entry->path().lexically_relative(scripts_dir).generic_string());
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

std::string in_scripts(const std::string& script) { return std::string(config::kScriptsDirName) + "/" + script; }

// "line 3: unexpected end" -> "scripts/coin.rb:3: unexpected end".
std::string located(const std::string& file, const std::string& error) {
  if (error.rfind(kLineWord, 0) == 0) return file + kLineJoiner + error.substr(std::string(kLineWord).size());
  return file + ": " + error;
}

std::optional<std::string> syntax_problem(RubyVm& vm, const fs::path& scripts_dir, const std::string& script) {
  std::string error;
  const auto code = read_text_file(scripts_dir / script, error);
  if (!code) return located(in_scripts(script), error);
  const RubyResult checked = vm.check_syntax(*code, in_scripts(script));
  if (checked.ok) return std::nullopt;
  return located(in_scripts(script), checked.error);
}

// Where in a game an object is: "scene main #7 Coin1".
std::string where(const std::string& scene_name, const SceneDesc& scene, int index) {
  return tr("project.check.where",
            {{"scene", scene_name}, {"object", object_id(index) + " " + scene.nodes[index].name}});
}

// The scenes' look at one game: what was already looked at is not looked at again.
struct SceneChecks {
  fs::path game_dir;
  std::vector<std::string> scripts;  // the game's
  std::set<std::string> classes_checked;
  CheckReport& report;
};

// What to tell someone whose script is not there: the names near it, else those there are.
std::string missing_script_hint(const SceneChecks& checks, const std::string& script) {
  const std::vector<std::string> near = closest_words(script, checks.scripts, kMostSuggestions);
  if (!near.empty()) return tr("project.check.did_you_mean", {{"names", joined(near)}});
  return tr("project.check.scripts_there", {{"names", checks.scripts.empty() ? "-" : joined(checks.scripts)}});
}

void check_class(SceneChecks& checks, const std::string& script, const std::string& place) {
  if (!checks.classes_checked.insert(script).second) return;
  std::string error;
  const auto code = read_text_file(checks.game_dir / config::kScriptsDirName / script, error);
  const std::string class_name = class_name_for_script(script);
  if (code && defines_class(*code, class_name)) return;
  checks.report.errors.push_back(
      tr("project.check.no_class", {{"script", in_scripts(script)}, {"class", class_name}, {"where", place}}));
}

void check_script(SceneChecks& checks, const std::string& script, const std::string& place) {
  const bool there = std::find(checks.scripts.begin(), checks.scripts.end(), script) != checks.scripts.end();
  if (there) {
    check_class(checks, script, place);
    return;
  }
  checks.report.errors.push_back(
      tr("project.check.missing_script",
         {{"where", place}, {"script", in_scripts(script)}, {"hint", missing_script_hint(checks, script)}}));
}

bool has_sky_picture(const fs::path& images_dir, const std::string& name) {
  return find_asset_file(images_dir, name, kSkyExtensions).has_value();
}

void check_sky(SceneChecks& checks, const PropertyMap& props, const std::string& place) {
  const auto sky = get_string(props, props::environment::kSkyImage);
  if (!sky || has_sky_picture(checks.game_dir / config::kImagesDirName, *sky)) return;
  checks.report.errors.push_back(tr("project.check.missing_sky", {{"where", place}, {"image", *sky}}));
}

void check_objects(SceneChecks& checks, const std::string& scene_name, const SceneDesc& scene) {
  for (int index = 0; index < static_cast<int>(scene.nodes.size()); ++index) {
    const PropertyMap& props = scene.nodes[index].props;
    const std::string place = where(scene_name, scene, index);
    for (const std::string& script : split_script_list(get_string(props, props::kScript).value_or(""))) {
      check_script(checks, script, place);
    }
    check_sky(checks, props, place);
  }
}

// The scene's file, as the game's folder holds it: "scene.glb", "scenes/level2.glb".
std::string scene_path(const fs::path& game_dir, const std::string& name) {
  return scene_file(game_dir, name).lexically_relative(game_dir).generic_string();
}

// "scene.glb: what is wrong with it".
std::string about(const std::string& file, const std::string& text) {
  std::string line = file;
  line += ": ";
  line += text;
  return line;
}

void check_scene(SceneChecks& checks, const std::string& name) {
  ++checks.report.scenes;
  const std::string file = scene_path(checks.game_dir, name);
  const LoadResult loaded = load_gltf_scene((checks.game_dir / file).string());
  for (const std::string& warning : loaded.warnings) checks.report.warnings.push_back(about(file, warning));
  if (!loaded.ok) {
    checks.report.errors.push_back(about(file, loaded.error));
    return;
  }
  check_objects(checks, name, loaded.scene);
}

bool is_name_letter(char letter) { return std::isalnum(static_cast<unsigned char>(letter)) != 0 || letter == '_'; }

// Whether `word` is in `line` at `at` (npos: nowhere).
bool has_at(const std::string& line, std::size_t at, const std::string& word) {
  return at != std::string::npos && line.compare(at, word.size(), word) == 0;
}

// Whether a name that ends at `at` ends there: the line does, or something no name holds follows.
bool name_ends_at(const std::string& line, std::size_t at) { return at == line.size() || !is_name_letter(line[at]); }

// Whether a line is `class <name>`, with nothing more to the name ("class Coin < Behaviour").
bool defines_on(const std::string& line, const std::string& class_name) {
  const std::size_t start = line.find_first_not_of(kBlanks);
  if (!has_at(line, start, kClassWord)) return false;
  const std::size_t name = line.find_first_not_of(kBlanks, start + std::strlen(kClassWord));
  return has_at(line, name, class_name) && name_ends_at(line, name + class_name.size());
}

}  // namespace

void check_scripts(const fs::path& game_dir, CheckReport& report) {
  RubyVm vm;
  const fs::path scripts_dir = game_dir / config::kScriptsDirName;
  for (const std::string& script : script_files(scripts_dir)) {
    ++report.scripts;
    if (const auto problem = syntax_problem(vm, scripts_dir, script)) report.errors.push_back(*problem);
  }
}

void check_scenes(const fs::path& game_dir, CheckReport& report) {
  SceneChecks checks{game_dir, script_files(game_dir / config::kScriptsDirName), {}, report};
  for (const std::string& name : scene_names(game_dir)) check_scene(checks, name);
}

bool defines_class(const std::string& code, const std::string& class_name) {
  std::stringstream lines(code);
  for (std::string line; std::getline(lines, line);) {
    if (defines_on(line, class_name)) return true;
  }
  return false;
}

}  // namespace redgem::tools
