#include "project/mcp_project_tools.hpp"

#include <algorithm>
#include <stdexcept>

#include "app/game_settings.hpp"
#include "core/config.hpp"
#include "editor/game_scenes.hpp"
#include "input/input_config.hpp"
#include "project/game_checks.hpp"
#include "project/game_templates.hpp"
#include "project/project_json_files.hpp"
#include "project/scene_command.hpp"

namespace redgem::tools::mcp {
namespace {

namespace fs = std::filesystem;
using project_files::Lines;

constexpr int kIndent = 2;
constexpr std::size_t kDefaultTexts = 50;
constexpr const char* kDefaultLanguage = "en";
constexpr const char* kDefaultTemplate = "empty";
constexpr const char* kInputSection = "input";
constexpr const char* kJsonExtension = ".json";

std::invalid_argument wrong(const std::string& what) { return std::invalid_argument(what); }

std::string joined(const Lines& lines) {
  std::string text;
  for (const auto& line : lines) text += line + "\n";
  return text;
}

std::string text_of(const Json& args, const char* key, const char* fallback) {
  return args.value(key, std::string(fallback));
}

// --- game.json -------------------------------------------------------------------------------

project_files::Findings findings_of(const CheckReport& report) { return {report.errors, report.warnings}; }

// What game.json says that the game cannot use, as `redgem check` says it, and its input
// section's bindings that read as nothing (errors: those bindings are dropped).
project_files::Findings settings_findings(const fs::path& game) {
  CheckReport report;
  check_game_file(game, report);
  check_menu(game, report);
  const Json settings = project_files::read_object(game / kGameSettingsFile);
  if (settings.contains(kInputSection)) input_map_from_json(settings[kInputSection], report.errors);
  return findings_of(report);
}

Lines all_of(const project_files::Findings& findings) {
  Lines lines = findings.errors;
  lines.insert(lines.end(), findings.warnings.begin(), findings.warnings.end());
  return lines;
}

// What a write leaves to look at: the warnings it brought, if any.
std::string warned(const Lines& warnings) { return warnings.empty() ? "" : "\nwarnings:\n" + joined(warnings); }

// The whole file, or the one section asked for ("menu": how the game's menus look).
Json settings_shown(const Json& file, const Json& args) {
  if (!args.contains("section")) return file;
  const std::string section = args["section"];
  if (!file.contains(section)) throw wrong("game.json has no " + section + " section");
  return file[section];
}

Json settings(const Options& options, const Json& args) {
  const Json file = project_files::read_object(options.game / kGameSettingsFile);
  const Lines problems = all_of(settings_findings(options.game));
  std::string text = settings_shown(file, args).dump(kIndent) + "\n";
  if (!problems.empty()) text += "problems:\n" + joined(problems);
  return tool_result(text);
}

// RFC 7386: each key of `changes` replaces the file's, an object merges into its object, and
// null takes the key away.
Json set_settings(const Options& options, const Json& args) {
  if (!args.contains("changes")) throw wrong("set_settings needs changes: what to merge into game.json");
  const fs::path file = options.game / kGameSettingsFile;
  Json merged = project_files::read_object(file);
  merged.merge_patch(args["changes"]);
  const Lines warnings = project_files::write_checked(file, merged, [&] { return settings_findings(options.game); });
  return tool_result("game.json is now:\n" + merged.dump(kIndent) + warned(warnings));
}

// --- locales ---------------------------------------------------------------------------------

std::string language_of(const Json& args) {
  const std::string language = text_of(args, "language", kDefaultLanguage);
  if (!project_files::valid_language(language)) throw wrong("language must be like en or pt-BR, not " + language);
  return language;
}

fs::path locale_file(const Options& options, const std::string& language) {
  return options.game / config::kLocalesDirName / (language + kJsonExtension);
}

bool starts_with(const std::string& text, const std::string& start) { return text.rfind(start, 0) == 0; }

std::string text_value(const Json& value) { return value.is_string() ? value.get<std::string>() : value.dump(); }

// "3 texts in locales/en.json", and how many are shown when not all are.
std::string texts_header(const std::string& language, std::size_t matched, std::size_t most) {
  std::string header = std::to_string(matched) + " texts in locales/" + language + ".json";
  if (matched > most) header += " (" + std::to_string(most) + " shown; narrow with prefix)";
  return header + "\n";
}

// The languages the game has texts in ("en", "pt-BR").
Lines languages(const Options& options) {
  Lines names;
  std::error_code missing;
  for (const auto& file : fs::directory_iterator(options.game / config::kLocalesDirName, missing)) {
    if (file.path().extension() == kJsonExtension) names.push_back(file.path().stem().string());
  }
  std::sort(names.begin(), names.end());
  return names;
}

std::string listed(const Lines& names) {
  std::string text;
  for (const auto& name : names) text += (text.empty() ? "" : ", ") + name;
  return text.empty() ? "none" : text;
}

Json texts(const Options& options, const Json& args) {
  const std::string language = language_of(args);
  if (!fs::exists(locale_file(options, language)))
    throw wrong("the game has no texts in " + language + "; it has: " + listed(languages(options)));
  const Json file = project_files::read_object(locale_file(options, language));
  const std::string prefix = text_of(args, "prefix", "");
  const std::size_t most = args.value("limit", kDefaultTexts);
  std::string text;
  std::size_t matched = 0;
  for (const auto& [key, value] : file.items()) {
    if (!starts_with(key, prefix)) continue;
    if (matched++ < most) text += key + " = " + text_value(value) + "\n";
  }
  return tool_result(texts_header(language, matched, most) + text);
}

// Each text a string, or null to take it away.
void check_texts(const Json& changes) {
  for (const auto& [key, value] : changes.items()) {
    if (!value.is_string() && !value.is_null()) throw wrong("texts." + key + " must be a text, or null to remove it");
  }
}

// What the game's check says of its dialogues and quests, whose texts are keys of the locales.
project_files::Findings story_findings(const fs::path& game) {
  CheckReport report;
  check_dialogues(game, report);
  check_quests(game, report);
  return findings_of(report);
}

Json set_texts(const Options& options, const Json& args) {
  if (!args.contains("texts")) throw wrong("set_texts needs texts: {\"key\": \"text\"}, null removing one");
  check_texts(args["texts"]);
  const std::string language = language_of(args);
  const fs::path file = locale_file(options, language);
  Json merged = project_files::read_object(file);
  merged.merge_patch(args["texts"]);
  const Lines warnings = project_files::write_checked(file, merged, [&] { return story_findings(options.game); });
  return tool_result("locales/" + language + ".json has " + std::to_string(merged.size()) + " texts" +
                     warned(warnings));
}

// --- dialogues, quests, items ----------------------------------------------------------------

const char* story_folder(const std::string& kind) {
  if (kind == "dialogue") return config::kDialoguesDirName;
  if (kind == "quest") return config::kQuestsDirName;
  if (kind == "item") return config::kItemsDirName;
  throw wrong("kind must be one of: dialogue, quest, item");
}

fs::path story_file(const Options& options, const Json& args) {
  const std::string name = text_of(args, "name", "");
  if (!project_files::valid_file_name(name)) throw wrong("name must be small letters, digits, _ and -, as npcs/baker");
  return options.game / story_folder(text_of(args, "kind", "")) / (name + kJsonExtension);
}

// The names of the files of a kind ("npcs/baker"), sorted.
Lines story_names(const fs::path& folder) {
  Lines names;
  std::error_code missing;
  for (fs::recursive_directory_iterator file(folder, missing), end; file != end; file.increment(missing)) {
    if (file->path().extension() != kJsonExtension) continue;
    names.push_back(fs::relative(file->path(), folder).replace_extension().generic_string());
  }
  std::sort(names.begin(), names.end());
  return names;
}

Json story(const Options& options, const Json& args) {
  const std::string kind = text_of(args, "kind", "");
  if (!args.contains("name")) return tool_result(joined(story_names(options.game / story_folder(kind))));
  const fs::path file = story_file(options, args);
  if (!fs::exists(file)) throw wrong("the game has no " + kind + " called " + args["name"].get<std::string>());
  return tool_result(project_files::read_object(file).dump(kIndent));
}

// Whether a call would replace a file without saying it means to.
bool replaces_unconfirmed(const fs::path& file, const Json& args) {
  return fs::exists(file) && !args.value("confirm", false);
}

Json set_story(const Options& options, const Json& args) {
  const fs::path file = story_file(options, args);
  if (!args.contains("content")) throw wrong("set_story needs content: the whole file, as JSON");
  if (replaces_unconfirmed(file, args)) throw wrong("that file is there already; replacing it needs confirm=true");
  const Lines warnings =
      project_files::write_checked(file, args["content"], [&] { return story_findings(options.game); });
  return tool_result("wrote " + fs::relative(file, options.game).generic_string() + warned(warnings));
}

// --- scenes and games ------------------------------------------------------------------------

Json scenes(const Options& options) { return tool_result(joined(scene_names(options.game))); }

Json new_scene(const Options& options, const Json& args) {
  std::string error;
  const auto file = create_scene(options.game, text_of(args, "scene", ""), error);
  if (!file) throw wrong(error);
  return tool_result("made " + fs::relative(*file, options.game).generic_string() +
                     " (a ground, a sun and a camera); open it with editor action=open_scene");
}

// A new game goes next to this one: the server's game folder is the one place it writes around.
fs::path new_game_folder(const Options& options, const Json& args) {
  const std::string name = text_of(args, "path", "");
  if (!valid_game_name(name)) throw wrong("path must be a folder name: letters, digits, spaces, - and _");
  const fs::path folder = fs::weakly_canonical(fs::absolute(options.game)).parent_path() / name;
  if (fs::exists(folder)) throw wrong(folder.string() + " is there already");
  return folder;
}

Json new_game(const Options& options, const Json& args) {
  const fs::path folder = new_game_folder(options, args);
  std::string error;
  if (!create_game(folder, text_of(args, "template", kDefaultTemplate), error)) throw wrong(error);
  return tool_result("made " + folder.string() + "; serve it with redgem mcp " + folder.string());
}

using Action = Json (*)(const Options&, const Json&);
struct Known {
  const char* name;
  Action run;
  bool reads;
};
constexpr Known kActions[] = {
    {"settings", settings, true},
    {"set_settings", set_settings, false},
    {"texts", texts, true},
    {"set_texts", set_texts, false},
    {"story", story, true},
    {"set_story", set_story, false},
    {"scenes", [](const Options& o, const Json&) { return scenes(o); }, true},
    {"new_scene", new_scene, false},
    {"new_game", new_game, false},
};

const Known* action_named(const std::string& name) {
  const auto* found = std::find_if(std::begin(kActions), std::end(kActions),
                                   [&name](const Known& known) { return name == known.name; });
  return found == std::end(kActions) ? nullptr : found;
}

}  // namespace

Json project_tool(const Options& options, const Json& args) {
  const Known* action = action_named(args.at("action").get<std::string>());
  if (action == nullptr) throw wrong("unknown project action");
  if (options.read_only && !action->reads) throw wrong("read-only mode only reads the project");
  return action->run(options, args);
}

bool reads_project(const std::string& action) {
  const Known* known = action_named(action);
  return known != nullptr && known->reads;
}

}  // namespace redgem::tools::mcp
