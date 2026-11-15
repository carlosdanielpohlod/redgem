#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "script/dialogue_file.hpp"

// A game's quest files: <game>/quests/<name>.json, each a quest's stages and what the players do in
// each (see ruby/engine/quests.rb for what one holds). The engine reads one as a script asks for it,
// and `redgem check` reads them all; both look it over the same way here.
namespace redgem::quest {

inline constexpr std::uintmax_t kMaxFileBytes = 1024 * 1024;
// A quest's state is kept in GameVariables ("quest.<name>", "quest.<name>#<objective>"), whose
// names are at most 64 bytes: that leaves a quest's name this many.
inline constexpr std::size_t kMaxNameBytes = 48;
inline constexpr std::size_t kMaxStageBytes = dialogue::kMaxVariableNameBytes;  // a stage is a variable's value
inline constexpr std::size_t kMaxObjectives = 16;                               // in one stage
inline constexpr std::size_t kMaxTargetBytes = 128;  // an object's, an item's or a check's name
inline constexpr int kMaxCount = 1000000;            // how many an objective may ask for

// A dialogue a quest names (a talk_to objective's), maybe with the node it must reach.
struct DialogueUse {
  std::string dialogue;
  std::string node;  // empty: any
  std::string where;
};

// A quest file, read and looked over.
struct QuestFile {
  nlohmann::json json;                   // as read (nothing when it is not JSON)
  std::vector<std::string> errors;       // what makes it unusable: "stage find: there is no stage \"bake\""
  std::vector<std::string> warnings;     // what may be a mistake: no stage ends the quest
  std::vector<dialogue::NameUse> texts;  // the I18n keys it shows
  std::vector<dialogue::NameUse> items;  // the items its objectives name
  std::vector<DialogueUse> dialogues;    // the dialogues they name
  std::set<std::string> stages;          // the ids of its stages

  bool ok() const { return errors.empty(); }
};

// Reads the text of a quest file and looks it over.
QuestFile read_quest(std::string_view text);

// The quest file of that name in a game, read and looked over; nothing when it cannot be read or is
// larger than kMaxFileBytes.
std::optional<QuestFile> read_quest_file(const std::filesystem::path& game_dir, std::string_view name);

// Whether a script may name a quest so: a name that stays in the quests folder ("bread",
// "village/bread"), short enough for the GameVariables that keep its state.
bool is_quest_name(std::string_view name);

// The file of a quest in a game: <game>/quests/<name>.json.
std::filesystem::path quest_file(const std::filesystem::path& game_dir, std::string_view name);

// How a message names a quest's file: "quests/bread.json".
std::string shown_file(std::string_view name);

// The quests of a game, by name ("village/bread"), sorted.
std::vector<std::string> quest_names(const std::filesystem::path& game_dir);

}  // namespace redgem::quest
