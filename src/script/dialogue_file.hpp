#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "script/game_json_files.hpp"

// A game's dialogue files: <game>/dialogues/<name>.json, each the lines of a conversation and the
// ways between them (see ruby/engine/dialogue.rb for what one holds). The engine reads one as a
// script asks for it, and `redgem check` reads them all; both look it over the same way here, so a
// script is only given a file that is well made.
namespace redgem::dialogue {

inline constexpr const char* kFileExtension = game_json::kFileExtension;
inline constexpr std::uintmax_t kMaxFileBytes = 1024 * 1024;
inline constexpr int kMaxDepth = game_json::kMaxDepth;
// What the network carries of a dialogue (ruby/engine/network_dialogues.rb): bytes in its name and
// in the id of a node. What a variable may be called and hold (ruby/engine/game_variables.rb).
inline constexpr std::size_t kMaxNameBytes = 128;
inline constexpr std::size_t kMaxNodeIdBytes = 128;
inline constexpr std::size_t kMaxVariableNameBytes = 64;
inline constexpr std::size_t kMaxVariableTextBytes = 1024;

// A name a dialogue file uses (an I18n key, a quest), and where in the file ("node hello choice 2").
struct NameUse {
  std::string name;
  std::string where;
};

// A quest's stage a dialogue file names (a condition's "is" or "not", an effect's "to").
struct StageUse {
  std::string quest;
  std::string stage;
  std::string where;
};

// A dialogue file, read and looked over.
struct DialogueFile {
  nlohmann::json json;                // as read (nothing when it is not JSON)
  std::vector<std::string> errors;    // what makes it unusable: "node hello: it has no text"
  std::vector<std::string> warnings;  // what may be a mistake: a node never reached
  std::vector<NameUse> texts;         // the I18n keys it shows
  std::vector<NameUse> quests;        // the quests its conditions and effects name
  std::vector<StageUse> stages;       // the stages of quests they name

  bool ok() const { return errors.empty(); }
};

// Reads the text of a dialogue file and looks it over.
DialogueFile read_dialogue(std::string_view text);

// Whether a script may name a dialogue so: a name that stays in the dialogues folder ("baker",
// "village/baker"), short enough to travel over the network.
bool is_dialogue_name(std::string_view name);

// The file of a dialogue in a game: <game>/dialogues/<name>.json.
std::filesystem::path dialogue_file(const std::filesystem::path& game_dir, std::string_view name);

// How a message names a dialogue's file: "dialogues/baker.json".
std::string shown_file(std::string_view name);

// The dialogues of a game, by name ("village/baker"), sorted.
std::vector<std::string> dialogue_names(const std::filesystem::path& game_dir);

}  // namespace redgem::dialogue
