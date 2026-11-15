#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

// What a game's folders of JSON files have in common (dialogues/*.json, quests/*.json): how one is
// parsed, and how the files of a folder are found by name.
namespace redgem::game_json {

inline constexpr const char* kFileExtension = ".json";
// How deep a file's JSON may nest (each level a Ruby value inside another, once it is read).
inline constexpr int kMaxDepth = 32;

// The JSON of a file's text; a discarded value when it is not JSON or nests deeper than kMaxDepth.
nlohmann::json parsed(std::string_view text);

// The JSON files of a folder of the game (and of the folders in it), by name ("village/baker"),
// sorted. None when the folder is not there or is reached through a link.
std::vector<std::string> names_in(const std::filesystem::path& game_dir, const char* folder_name);

}  // namespace redgem::game_json
