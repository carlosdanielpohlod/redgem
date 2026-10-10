#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace redgem {

// Script names are snake_case file names: "door", "enemy_ai" (".rb" optional).
bool valid_script_name(const std::string& name);

// "enemy_ai" or "enemy_ai.rb" -> "enemy_ai.rb".
std::string script_file_name(const std::string& name);

// Ruby source of a new behaviour, ready to fill in.
std::string script_template(const std::string& file_name);

// Creates scripts_dir/<name>.rb from the template. Never overwrites an existing file.
// Returns the file name ("enemy_ai.rb") or nullopt with `error` set.
std::optional<std::string> create_script(const std::filesystem::path& scripts_dir, const std::string& name,
                                         std::string& error);

std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string& error);
// Opens a file or folder with the desktop's default application (xdg-open), without waiting.
void open_in_system(const std::filesystem::path& path);

bool write_text_file(const std::filesystem::path& path, const std::string& text, std::string& error);

}  // namespace redgem
