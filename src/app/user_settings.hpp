#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "render/gpu_select.hpp"

namespace redgem {

// Preferences of this user on this machine (not of a game): ~/.config/redgem/settings.json.
struct UserSettings {
  GpuChoice gpu = GpuChoice::Auto;
  std::string language;  // what the engine's texts are shown in ("pt"); empty: the system's
};

std::filesystem::path user_settings_file();
// Missing or unreadable files give the defaults.
UserSettings read_user_settings(const std::filesystem::path& file);
bool write_user_settings(const std::filesystem::path& file, const UserSettings& settings, std::string& error);

// The card to use: what the command line asked for, else the REDGEM_GPU environment variable,
// else the saved setting, else Auto. Unknown names are ignored.
GpuChoice resolve_gpu_choice(const std::optional<std::string>& command_line,
                             const std::optional<std::string>& environment, const UserSettings& settings);

// The card for this run of the program: what its command line asked for, else what the user's
// environment (REDGEM_GPU, read here and nowhere else) or saved settings say. Whoever opens a
// window is handed the answer.
GpuChoice gpu_choice_for_run(const std::optional<std::string>& command_line);

// The language for this run of the program: the one the user chose, else the system's.
std::string language_for_run();

}  // namespace redgem
