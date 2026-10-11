#pragma once

#include <filesystem>
#include <optional>
#include <string>

// Reading and writing whole files, and the paths of folders: what every part of the engine that
// touches the disk shares.
namespace redgem {

// What a file holds; nullopt, with `error` saying so, when it cannot be read.
std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string& error);
// Replaces what a file holds. False, with `error` saying so, when it cannot be written.
bool write_text_file(const std::filesystem::path& path, const std::string& text, std::string& error);

// A folder path as the rest of the code expects it: no "." or "..", no trailing separator
// ("games/foo/" -> "games/foo", whose filename is the game's name).
std::filesystem::path normalized_folder(const std::filesystem::path& folder);

}  // namespace redgem
