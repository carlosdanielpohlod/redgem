// What `redgem check` says of a game's quest files (see game_checks.hpp).

#include <map>
#include <set>
#include <string>

#include "core/config.hpp"
#include "core/game_files.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"
#include "project/game_checks.hpp"
#include "project/text_checks.hpp"
#include "script/dialogue_file.hpp"
#include "script/game_json_files.hpp"
#include "script/game_locales.hpp"
#include "script/quest_file.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;

// What the quests of one game are checked against.
struct QuestChecks {
  fs::path game_dir;
  GameLocales locales;
  std::map<std::string, std::set<std::string>> dialogues;  // the nodes of each dialogue (none: it does not read)
  bool has_items = false;                                  // a game with no items folder may name any item
  std::set<std::string> items;
  CheckReport& report;
};

std::set<std::string> nodes_of(const fs::path& game_dir, const std::string& name) {
  const auto bytes = read_file_up_to(dialogue::dialogue_file(game_dir, name), dialogue::kMaxFileBytes);
  if (!bytes) return {};
  const dialogue::DialogueFile read =
      dialogue::read_dialogue({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
  std::set<std::string> nodes;
  if (!read.ok()) return nodes;
  for (const auto& [id, node] : read.json.at("nodes").items()) nodes.insert(id);
  return nodes;
}

void error(const QuestChecks& checks, const char* key, const TextValues& values) {
  checks.report.errors.push_back(tr(key, values));
}

// (A dialogue that does not read has no nodes here: what is wrong with it is said elsewhere.)
void check_dialogue_use(const QuestChecks& checks, const std::string& file, const quest::DialogueUse& use) {
  const auto dialogue = checks.dialogues.find(use.dialogue);
  if (dialogue == checks.dialogues.end()) {
    return error(checks, "project.check.unknown_dialogue",
                 {{"file", file}, {"where", use.where}, {"dialogue", use.dialogue}});
  }
  if (use.node.empty() || dialogue->second.empty() || dialogue->second.count(use.node) != 0) return;
  error(checks, "project.check.unknown_node",
        {{"file", file}, {"where", use.where}, {"dialogue", use.dialogue}, {"node", use.node}});
}

void check_items(const QuestChecks& checks, const std::string& file, const quest::QuestFile& read) {
  if (!checks.has_items) return;
  for (const dialogue::NameUse& item : read.items) {
    if (checks.items.count(item.name) != 0) continue;
    error(checks, "project.check.unknown_item", {{"file", file}, {"where", item.where}, {"item", item.name}});
  }
}

void check_quest(const QuestChecks& checks, const std::string& name) {
  const std::string file = quest::shown_file(name);
  if (!quest::is_quest_name(name)) {
    return error(checks, "project.check.quest_name", {{"file", file}, {"bytes", std::to_string(quest::kMaxNameBytes)}});
  }
  const std::optional<quest::QuestFile> read = quest::read_quest_file(checks.game_dir, name);
  if (!read) {
    return error(checks, "project.check.quest_unreadable",
                 {{"file", file}, {"bytes", std::to_string(quest::kMaxFileBytes)}});
  }
  add_lines(checks.report.errors, file, read->errors);
  add_lines(checks.report.warnings, file, read->warnings);
  check_texts(checks.locales, checks.report, file, read->texts);
  for (const quest::DialogueUse& use : read->dialogues) check_dialogue_use(checks, file, use);
  check_items(checks, file, *read);
}

QuestChecks checks_for(const fs::path& game_dir, CheckReport& report) {
  QuestChecks checks{game_dir, read_game_locales(game_dir), {}, false, {}, report};
  for (const std::string& name : dialogue::dialogue_names(game_dir)) checks.dialogues[name] = nodes_of(game_dir, name);
  checks.has_items = game_files::is_folder(game_dir / config::kItemsDirName);
  for (const std::string& item : game_json::names_in(game_dir, config::kItemsDirName)) checks.items.insert(item);
  return checks;
}

}  // namespace

void check_quests(const fs::path& game_dir, CheckReport& report) {
  const std::vector<std::string> names = quest::quest_names(game_dir);
  if (names.empty()) return;
  const QuestChecks checks = checks_for(game_dir, report);
  for (const std::string& name : names) check_quest(checks, name);
}

}  // namespace redgem::tools
