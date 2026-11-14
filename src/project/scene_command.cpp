#include "project/scene_command.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>
#include <variant>

#include "app/scene_files.hpp"
#include "core/command_line.hpp"
#include "core/config.hpp"
#include "core/translations.hpp"
#include "editor/document/glb_compact.hpp"
#include "editor/document/scene_document.hpp"
#include "project/closest_words.hpp"
#include "project/scene_assignments.hpp"
#include "project/scene_lookup.hpp"
#include "project/scene_text.hpp"
#include "project/scene_diff.hpp"
#include "project/scene_revision.hpp"
#include "scene/props.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kSceneOption = "--scene";
constexpr const char* kFromOption = "--from";
constexpr const char* kParentOption = "--parent";
constexpr const char* kDepthOption = "--depth";
constexpr const char* kLimitOption = "--limit";
constexpr const char* kAgainstOption = "--against";
constexpr const char* kRevisionOption = "--revision";
constexpr const char* kDiffWord = "diff";
constexpr const char* kCompactWord = "compact";
// What the command takes after its name.
ToolGrammar grammar() {
  return {{kSceneOption, kFromOption, kParentOption, kDepthOption, kLimitOption, kAgainstOption, kRevisionOption},
          {command_line::kHelpOption}};
}
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

// Saves, and says what the object `index` is now, under its persistent identity.
ToolResult saved_object(OpenScene& scene, int index, const std::vector<std::string>& notes) {
  const SceneDesc& desc = scene.document.desc();
  ToolResult result = saved(scene, object_line(desc, index) + "\n");
  if (result.exit_code != kExitSuccess) return result;
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

SceneEdit edit_of(OpenScene& scene) { return SceneEdit{scene.document, scene.game_dir / config::kScriptsDirName, {}}; }

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
  const std::string problem = name_problem(object_word(scene));
  if (!problem.empty()) return failed(problem);
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
  const std::string line = object_id(scene.document.desc(), index) + " " + scene.document.node(index).name;
  scene.document.remove(index);
  const std::size_t gone = before - scene.document.desc().nodes.size();
  return saved(scene, tr("project.scene.removed", {{"object", line}, {"count", std::to_string(gone)}}) + "\n");
}

// The earlier save a diff compares with: what --against names, when it is one (no --revision does).
std::optional<std::string> earlier_save(const ToolLine& line) {
  std::optional<std::string> against = line.value(kAgainstOption);
  if (!against) return std::nullopt;
  if (!names_earlier_save(*against)) return std::nullopt;
  return against;
}

// Whether the line names what scene diff compares with: --against or --revision.
bool names_baseline(const ToolLine& line) {
  return line.value(kAgainstOption).has_value() || line.value(kRevisionOption).has_value();
}

ToolResult diff_with(OpenScene& scene, std::size_t limit) {
  const SceneDesc& desc = scene.document.desc();
  const ToolLine& line = scene.line;
  const std::optional<std::string> save = earlier_save(line);
  if (save) return compare_scene_file(desc, scene.name, *save, limit);
  const std::string revision = line.value(kRevisionOption).value_or(line.value(kAgainstOption).value_or(std::string()));
  return compare_scene_revision(desc, scene.name, scene.document.path(), revision, limit);
}

ToolResult diff(OpenScene& scene) {
  const bool against = scene.line.value(kAgainstOption).has_value();
  if (against == scene.line.value(kRevisionOption).has_value())
    return failed(tr("project.scene.diff_needs_against"), kExitUsage);
  const auto limit = count_option(scene.line, kLimitOption, kDefaultDiffLines);
  if (!limit) return failed(tr("project.scene.needs_count"), kExitUsage);
  return diff_with(scene, *limit);
}

// Whether the scene's file, as saved, is one compact can work on (see compactable).
bool saved_compactable(const std::filesystem::path& file) {
  std::string error;
  const std::optional<GlbFile> saved = read_glb(file, error);
  return saved && compactable(*saved);
}

// Saves the scene as it is: what it holds twice is written once, and what no object uses is left out.
ToolResult compact(OpenScene& scene) {
  std::string error;
  if (!scene.document.save(error)) return failed(error);
  const SceneDocument::SaveReport& report = scene.document.last_save();
  ToolResult result = answer(tr("project.scene.compacted", {{"scene", scene.name},
                                                            {"written", std::to_string(report.written_bytes)},
                                                            {"left_out", std::to_string(report.left_out_bytes)}}) +
                             "\n");
  if (!saved_compactable(scene.document.path())) result.err = std::string(tr("project.scene.compact_whole")) + "\n";
  return result;
}

// A line that takes as many words as it wants after those it needs (what to set).
constexpr std::size_t kAnyMore = std::numeric_limits<std::size_t>::max();

struct Known {
  const char* word;
  Subcommand run;
  std::size_t least;  // how many words it needs: the game, and the object it is about
  std::size_t most;   // how many it takes (a word too many is refused, never left unread)
};
constexpr Known kSubcommands[] = {
    {"list", list, kObjectWord, kObjectWord},          {kDiffWord, diff, kObjectWord, kObjectWord},
    {"get", get, kFirstAssignment, kFirstAssignment},  {"set", set, kFirstAssignment, kAnyMore},
    {"add", add, kFirstAssignment, kAnyMore},          {"remove", remove, kFirstAssignment, kFirstAssignment},
    {kCompactWord, compact, kObjectWord, kObjectWord},
};

// The options diff and compact have no use for.
constexpr const char* kDiffRefuses[] = {kFromOption, kParentOption, kDepthOption};
constexpr const char* kCompactRefuses[] = {kFromOption,  kParentOption,  kDepthOption,
                                           kLimitOption, kAgainstOption, kRevisionOption};

// The refusal (`message`) of the first of `options` the line gives; "" when it gives none.
template <std::size_t N>
std::string first_refused(const ToolLine& line, const char* const (&options)[N], const char* message) {
  for (const char* option : options) {
    if (line.value(option)) return tr(message, {{"option", option}});
  }
  return {};
}

std::string unsupported_option(const Known& subcommand, const ToolLine& line) {
  const std::string word = subcommand.word;
  if (word == kCompactWord) return first_refused(line, kCompactRefuses, "project.scene.compact_bad_option");
  const bool diff_command = word == kDiffWord;
  if (!diff_command && names_baseline(line)) return tr("project.scene.diff_against_only");
  if (!diff_command) return {};
  return first_refused(line, kDiffRefuses, "project.scene.diff_bad_option");
}

bool has_its_words(const Known& subcommand, const ToolLine& line) {
  return line.words.size() >= subcommand.least && line.words.size() <= subcommand.most;
}

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
  if (!has_its_words(subcommand, line)) return failed(usage(), kExitUsage);
  const std::string option_error = unsupported_option(subcommand, line);
  if (!option_error.empty()) return failed(option_error, kExitUsage);
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

bool asks_for_help(const std::vector<std::string>& args) {
  return !args.empty() && command_line::asks_for_help(args.front());
}

ToolResult run_scene_command(const std::vector<std::string>& args) {
  if (asks_for_help(args)) return answer(usage());
  const Known* subcommand = args.empty() ? nullptr : subcommand_named(args.front());
  if (subcommand == nullptr) return failed(usage(), kExitUsage);
  std::string error;
  auto line = read_tool_line(args, 1, grammar(), error);
  if (!line) return failed(error + "\n\n" + usage(), kExitUsage);
  return run_subcommand(*subcommand, std::move(*line));
}

}  // namespace redgem::tools
