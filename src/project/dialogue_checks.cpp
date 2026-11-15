// What `redgem check` says of a game's dialogue files (see game_checks.hpp).

#include <map>
#include <set>
#include <string>

#include "audio/audio.hpp"
#include "core/config.hpp"
#include "core/game_files.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"
#include "project/game_checks.hpp"
#include "project/text_checks.hpp"
#include "script/dialogue_file.hpp"
#include "script/game_locales.hpp"
#include "script/quest_file.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;

// What the dialogues of one game are checked against.
struct DialogueChecks {
  fs::path game_dir;
  GameLocales locales;
  bool has_quests;               // a game with no quests folder keeps its quests in GameVariables: any name will do
  std::set<std::string> quests;  // the quests it has
  std::map<std::string, std::set<std::string>> stages;  // the stages of each that reads
  CheckReport& report;
};

void check_quests(const DialogueChecks& checks, const std::string& file, const dialogue::DialogueFile& dialogue) {
  if (!checks.has_quests) return;
  for (const dialogue::NameUse& quest : dialogue.quests) {
    if (checks.quests.count(quest.name) != 0) continue;
    checks.report.errors.push_back(
        tr("project.check.unknown_quest", {{"file", file}, {"where", quest.where}, {"quest", quest.name}}));
  }
}

bool is_unknown_stage(const DialogueChecks& checks, const dialogue::StageUse& use) {
  const auto stages = checks.stages.find(use.quest);
  return stages != checks.stages.end() && stages->second.count(use.stage) == 0;
}

// (A quest that is not there, or does not read, is said elsewhere.)
void check_stages(const DialogueChecks& checks, const std::string& file, const dialogue::DialogueFile& dialogue) {
  for (const dialogue::StageUse& use : dialogue.stages) {
    if (!is_unknown_stage(checks, use)) continue;
    checks.report.errors.push_back(
        tr("project.check.unknown_stage",
           {{"file", file}, {"where", use.where}, {"quest", use.quest}, {"stage", use.stage}}));
  }
}

// A line whose voice has no sound file plays with no voice: a warning.
void check_voices(const DialogueChecks& checks, const std::string& file, const dialogue::DialogueFile& dialogue) {
  const fs::path sounds = checks.game_dir / config::kSoundsDirName;
  for (const dialogue::NameUse& voice : dialogue.voices) {
    if (find_sound_file(sounds, voice.name)) continue;
    checks.report.warnings.push_back(
        tr("project.check.missing_voice", {{"file", file}, {"where", voice.where}, {"voice", voice.name}}));
  }
}

void check_loaded_dialogue(const DialogueChecks& checks, const std::string& file, const dialogue::DialogueFile& read) {
  add_lines(checks.report.errors, file, read.errors);
  add_lines(checks.report.warnings, file, read.warnings);
  check_texts(checks.locales, checks.report, file, read.texts);
  check_quests(checks, file, read);
  check_stages(checks, file, read);
  check_voices(checks, file, read);
}

void check_dialogue(const DialogueChecks& checks, const std::string& name) {
  const std::string file = dialogue::shown_file(name);
  const auto bytes = read_file_up_to(dialogue::dialogue_file(checks.game_dir, name), dialogue::kMaxFileBytes);
  if (!bytes) {
    checks.report.errors.push_back(
        tr("project.check.dialogue_unreadable", {{"file", file}, {"bytes", std::to_string(dialogue::kMaxFileBytes)}}));
    return;
  }
  const dialogue::DialogueFile read =
      dialogue::read_dialogue({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
  check_loaded_dialogue(checks, file, read);
}

void add_quests(DialogueChecks& checks) {
  for (const std::string& name : quest::quest_names(checks.game_dir)) {
    checks.quests.insert(name);
    const std::optional<quest::QuestFile> read = quest::read_quest_file(checks.game_dir, name);
    if (read && read->ok()) checks.stages[name] = read->stages;
  }
}

}  // namespace

void check_dialogues(const fs::path& game_dir, CheckReport& report) {
  const std::vector<std::string> names = dialogue::dialogue_names(game_dir);
  if (names.empty()) return;
  const bool has_quests = game_files::is_folder(game_dir / config::kQuestsDirName);
  DialogueChecks checks{game_dir, read_game_locales(game_dir), has_quests, {}, {}, report};
  add_quests(checks);
  for (const std::string& name : names) check_dialogue(checks, name);
}

}  // namespace redgem::tools
