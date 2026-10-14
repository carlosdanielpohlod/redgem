#pragma once

#include <filesystem>

#include <nlohmann/json.hpp>

namespace redgem {

// What a JSON file holds; a discarded value (no object, no array) when it is missing or broken.
// What the engine keeps in such files (a game's settings, the user's, the games opened lately)
// is all optional: whoever reads one goes on with its defaults.
nlohmann::json read_json_file(const std::filesystem::path& file);

// Whether what a file holds says something under `key`, as a text or as a whole number: anything
// else there was not written by the engine, and is not used.
bool holds_text(const nlohmann::json& json, const char* key);
bool holds_whole_number(const nlohmann::json& json, const char* key);

}  // namespace redgem
