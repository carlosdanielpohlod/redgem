#include "project/scene_command.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>
#include <variant>

#include "app/scene_files.hpp"
#include "core/command_line.hpp"
#include "core/config.hpp"
#include "core/translations.hpp"
#include "editor/document/scene_document.hpp"
#include "project/closest_words.hpp"
#include "project/scene_assignments.hpp"
#include "project/scene_lookup.hpp"
#include "project/scene_text.hpp"
#include "scene/props.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kSceneOption = "--scene";
constexpr const char* kFromOption = "--from";
constexpr const char* kParentOption = "--parent";
constexpr const char* kDepthOption = "--depth";
constexpr const char* kLimitOption = "--limit";
const ToolGrammar kGrammar{{kSceneOption, kFromOption, kParentOption, kDepthOption, kLimitOption},
                           {command_line::kHelpOption}};
// A list stops after this many lines unless --limit says otherwise (0: never).
constexpr std::size_t kDefaultListLimit = 200;
// The words of the line: the game, then the object (or the name of a new one), then what to set.
constexpr std::size_t kGameWord = 0;
constexpr std::size_t kObjectWord = 1;
constexpr std::size_t kFirstAssignment = 2;

// A scene of a game, open, and what the command line said about it.
struct OpenScene {
  SceneDocument document;
  std::string name;
  std::filesystem::path game_dir;
  ToolLine line;
};

using Subcommand = ToolResult (*)(OpenScene&);

std::string usage() { return tr("project.scene.usage"); }

ToolResult answer(std::string text) {
  ToolResult result;
  result.out = std::move(text);
  return result;
}

ToolResult saved(OpenScene& scene, std::string text) {
  std::string error;
  if (!scene.document.save(error)) return failed(error);
  return answer(std::move(text));
}

// Saves, and says what the object `index` is now, as the saved file has it.
ToolResult saved_object(OpenScene& scene, int index, const std::vector<std::string>& notes) {
  const int id = saved_id(scene.document.desc(), index);
  std::string error;
  if (!scene.document.save(error)) return failed(error);
  auto reread = SceneDocument::open(scene.document.path());
  if (auto* problem = std::get_if<std::string>(&reread)) return failed(*problem);
  ToolResult result = answer(object_line(std::get<SceneDocument>(reread).desc(), id) + "\n");
  for (const std::string& note : notes) result.err += note + "\n";
  return result;
}

const std::string& object_word(const OpenScene& scene) { return scene.line.words[kObjectWord]; }

// The object the line names; why none, as the result, when it names none.
std::variant<int, ToolResult> named_object(const OpenScene& scene) {
  const FoundObject found = find_object(scene.document.desc(), object_word(scene));
  if (found.index == kNoIndex) return failed(found.error);
  return found.index;
}

// What a count must be: a whole number, not below zero.
bool is_count(double number) { return number >= 0 && number == static_cast<double>(static_cast<std::size_t>(number)); }

// The count an option is given: `otherwise` without the option, nothing when it is no count.
std::optional<std::size_t> count_option(const ToolLine& line, const char* option, std::size_t otherwise) {
  const auto value = line.value(option);
  if (!value) return otherwise;
  const auto number = number_from(*value);
  if (!number || !is_count(*number)) return std::nullopt;
  return static_cast<std::size_t>(*number);
}

ToolResult list(OpenScene& scene) {
  const auto depth = count_option(scene.line, kDepthOption, kEveryLevel);
  const auto most = count_option(scene.line, kLimitOption, kDefaultListLimit);
  if (!depth || !most) return failed(tr("project.scene.needs_count"));
  return answer(scene_list(scene.document.desc(), scene.name, ListLimits{*depth, *most}));
}

ToolResult get(OpenScene& scene) {
  const auto object = named_object(scene);
  if (const auto* problem = std::get_if<ToolResult>(&object)) return *problem;
  return answer(object_detail(scene.document.desc(), std::get<int>(object)));
}

// Applies every word from the first assignment on to `index`; what went wrong, if anything did.
std::string assign_all(OpenScene& scene, SceneEdit& edit, int& index) {
  const std::vector<std::string>& words = scene.line.words;
  for (std::size_t word = kFirstAssignment; word < words.size(); ++word) {
    const std::string problem = apply_assignment(edit, index, words[word]);
    if (!problem.empty()) return problem;
  }
  return {};
}

SceneEdit edit_of(OpenScene& scene) {
  return SceneEdit{scene.document, scene.game_dir / config::kScriptsDirName, {}};
}

// Sets what the line says on `index`, and saves.
ToolResult assign_and_save(OpenScene& scene, int index) {
  SceneEdit edit = edit_of(scene);
  const std::string problem = assign_all(scene, edit, index);
  if (!problem.empty()) return failed(problem);
  return saved_object(scene, index, edit.notes);
}

ToolResult set(OpenScene& scene) {
  const auto object = named_object(scene);
  if (const auto* problem = std::get_if<ToolResult>(&object)) return *problem;
  if (scene.line.words.size() <= kFirstAssignment) return failed(tr("project.scene.nothing_to_set"));
  return assign_and_save(scene, std::get<int>(object));
}

// The object an option names; kNoIndex when the option is not given.
std::variant<int, ToolResult> optional_object(const OpenScene& scene, const char* option) {
  const auto reference = scene.line.value(option);
  if (!reference) return kNoIndex;
  const FoundObject found = find_object(scene.document.desc(), *reference);
  if (found.index == kNoIndex) return failed(found.error);
  return found.index;
}

// A copy of a template object; a copy of a prefab is an object of the scene, not a template.
int copy_of(SceneDocument& document, int source, const std::string& name) {
  const int copy = document.duplicate(source);
  document.set_name(copy, document.unique_name(name));
  document.remove_prop(copy, props::kPrefab);
  return copy;
}

int added(OpenScene& scene, int source, int parent) {
  SceneDocument& document = scene.document;
  if (source == kNoIndex) return document.add_empty(object_word(scene), parent);
  const int copy = copy_of(document, source, object_word(scene));
  return scene.line.value(kParentOption) ? document.reparent(copy, parent) : copy;
}

ToolResult add(OpenScene& scene) {
  const auto source = optional_object(scene, kFromOption);
  if (const auto* problem = std::get_if<ToolResult>(&source)) return *problem;
  const auto parent = optional_object(scene, kParentOption);
  if (const auto* problem = std::get_if<ToolResult>(&parent)) return *problem;
  return assign_and_save(scene, added(scene, std::get<int>(source), std::get<int>(parent)));
}

ToolResult remove(OpenScene& scene) {
  const auto object = named_object(scene);
  if (const auto* problem = std::get_if<ToolResult>(&object)) return *problem;
  const int index = std::get<int>(object);
  const std::size_t before = scene.document.desc().nodes.size();
  const std::string line = object_id(index) + " " + scene.document.node(index).name;
  scene.document.remove(index);
  const std::size_t gone = before - scene.document.desc().nodes.size();
  return saved(scene, tr("project.scene.removed", {{"object", line}, {"count", std::to_string(gone)}}) + "\n");
}

struct Known {
  const char* word;
  Subcommand run;
  std::size_t words;  // how many words it needs at least: the game, and the object it is about
};
constexpr Known kSubcommands[] = {
    {"list", list, kObjectWord}, {"get", get, kFirstAssignment},       {"set", set, kFirstAssignment},
    {"add", add, kFirstAssignment}, {"remove", remove, kFirstAssignment},
};

const Known* subcommand_named(const std::string& word) {
  const auto* found = std::find_if(std::begin(kSubcommands), std::end(kSubcommands),
                                   [&word](const Known& known) { return word == known.word; });
  return found == std::end(kSubcommands) ? nullptr : found;
}

std::variant<OpenScene, ToolResult> open_scene(ToolLine line) {
  const std::filesystem::path game_dir = line.words[kGameWord];
  const std::string name = line.value(kSceneOption).value_or(config::kMainSceneName);
  std::string error;
  const auto file = named_scene_file(game_dir, name, error);
  if (!file) return failed(error);
  auto opened = SceneDocument::open(*file);
  if (auto* problem = std::get_if<std::string>(&opened)) return failed(*problem);
  return OpenScene{std::move(std::get<SceneDocument>(opened)), name, game_dir, std::move(line)};
}

ToolResult run_subcommand(const Known& subcommand, ToolLine line) {
  if (line.has(command_line::kHelpOption)) return answer(usage());
  if (line.words.size() < subcommand.words) return failed(usage(), kExitUsage);
  auto opened = open_scene(std::move(line));
  if (auto* problem = std::get_if<ToolResult>(&opened)) return *problem;
  return subcommand.run(std::get<OpenScene>(opened));
}

void add_scenes_in(const std::filesystem::path& folder, std::vector<std::string>& names) {
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
    if (entry.path().extension() == config::kSceneExtension) names.push_back(entry.path().stem().string());
  }
}

}  // namespace

std::vector<std::string> scene_names(const std::filesystem::path& game_dir) {
  std::vector<std::string> names;
  add_scenes_in(game_dir / config::kScenesDirName, names);
  std::sort(names.begin(), names.end());
  if (is_game_folder(game_dir)) names.insert(names.begin(), config::kMainSceneName);
  return names;
}

std::optional<std::filesystem::path> named_scene_file(const std::filesystem::path& game_dir, const std::string& name,
                                                      std::string& error) {
  const std::vector<std::string> names = scene_names(game_dir);
  if (std::find(names.begin(), names.end(), name) != names.end()) return scene_file(game_dir, name);
  if (names.empty()) {
    error = tr("project.tools.not_a_game", {{"game_dir", game_dir.string()}});
  } else {
    error = tr("project.scene.no_scene", {{"scene", name}, {"scenes", joined(names)}});
  }
  return std::nullopt;
}

ToolResult run_scene_command(const std::vector<std::string>& args) {
  const Known* subcommand = args.empty() ? nullptr : subcommand_named(args.front());
  if (subcommand == nullptr) return failed(usage(), kExitUsage);
  std::string error;
  auto line = read_tool_line(args, 1, kGrammar, error);
  if (!line) return failed(error + "\n\n" + usage(), kExitUsage);
  return run_subcommand(*subcommand, std::move(*line));
}

}  // namespace redgem::tools
