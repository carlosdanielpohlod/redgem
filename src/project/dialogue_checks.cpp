// What `redgem check` says of a game's dialogue files (see game_checks.hpp).

#include <set>
#include <string>

#include "core/config.hpp"
#include "core/game_files.hpp"
#include "core/language_names.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"
#include "project/game_checks.hpp"
#include "script/dialogue_file.hpp"
#include "script/game_locales.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;
constexpr const char* kQuestExtension = ".json";

// What the dialogues of one game are checked against.
struct DialogueChecks {
  fs::path game_dir;
  GameLocales locales;
  bool has_quests;  // a game with no quests folder keeps its quests in GameVariables: any name will do
  CheckReport& report;
};

// "dialogues/baker.json".
std::string shown_file(const std::string& name) {
  return std::string(config::kDialoguesDirName) + "/" + name + dialogue::kFileExtension;
}

bool game_has_text(const DialogueChecks& checks, const std::string& language, const std::string& key) {
  return checks.locales.texts.find(language_key(language), key) != nullptr;
}

bool has_text_anywhere(const DialogueChecks& checks, const std::string& key) {
  for (const std::string& language : checks.locales.languages) {
    if (game_has_text(checks, language, key)) return true;
  }
  return translations::all().find(kFallbackLanguage, key) != nullptr;
}

// A key with no text anywhere is an error; one a language of the game lacks, a warning for it.
void check_text(const DialogueChecks& checks, const std::string& file, const dialogue::NameUse& text) {
  if (!has_text_anywhere(checks, text.name)) {
    checks.report.errors.push_back(
        tr("project.check.missing_text", {{"file", file}, {"where", text.where}, {"key", text.name}}));
    return;
  }
  for (const std::string& language : checks.locales.languages) {
    if (game_has_text(checks, language, text.name)) continue;
    checks.report.warnings.push_back(
        tr("project.check.missing_translation",
           {{"file", file}, {"where", text.where}, {"key", text.name}, {"language", language}}));
  }
}

void check_texts(const DialogueChecks& checks, const std::string& file, const dialogue::DialogueFile& dialogue) {
  std::set<std::string> seen;  // (a key used twice is said once)
  for (const dialogue::NameUse& text : dialogue.texts) {
    if (seen.insert(text.name).second) check_text(checks, file, text);
  }
}

bool is_quest(const DialogueChecks& checks, const std::string& quest) {
  if (!safe_asset_name(quest)) return false;
  return game_files::is_file(checks.game_dir / config::kQuestsDirName / (quest + kQuestExtension));
}

void check_quests(const DialogueChecks& checks, const std::string& file, const dialogue::DialogueFile& dialogue) {
  if (!checks.has_quests) return;
  for (const dialogue::NameUse& quest : dialogue.quests) {
    if (is_quest(checks, quest.name)) continue;
    checks.report.errors.push_back(
        tr("project.check.unknown_quest", {{"file", file}, {"where", quest.where}, {"quest", quest.name}}));
  }
}

void add_lines(std::vector<std::string>& to, const std::string& file, const std::vector<std::string>& lines) {
  for (const std::string& line : lines) to.push_back(file + ": " + printable(line));
}

void check_dialogue(const DialogueChecks& checks, const std::string& name) {
  const std::string file = shown_file(name);
  const auto bytes = read_file_up_to(dialogue::dialogue_file(checks.game_dir, name), dialogue::kMaxFileBytes);
  if (!bytes) {
    checks.report.errors.push_back(
        tr("project.check.dialogue_unreadable", {{"file", file}, {"bytes", std::to_string(dialogue::kMaxFileBytes)}}));
    return;
  }
  const dialogue::DialogueFile read =
      dialogue::read_dialogue({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
  add_lines(checks.report.errors, file, read.errors);
  add_lines(checks.report.warnings, file, read.warnings);
  check_texts(checks, file, read);
  check_quests(checks, file, read);
}

}  // namespace

void check_dialogues(const fs::path& game_dir, CheckReport& report) {
  const std::vector<std::string> names = dialogue::dialogue_names(game_dir);
  if (names.empty()) return;
  const bool has_quests = game_files::is_folder(game_dir / config::kQuestsDirName);
  const DialogueChecks checks{game_dir, read_game_locales(game_dir), has_quests, report};
  for (const std::string& name : names) check_dialogue(checks, name);
}

}  // namespace redgem::tools
