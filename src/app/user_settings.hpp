#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace redgem {

// Which graphics card draws the editor and the games on machines that have two.
enum class GpuChoice {
  Auto,        // the dedicated card when there is one (the default)
  Dedicated,   // same as Auto today; kept apart so the choice survives a change of default
  Integrated,  // the low-power card: cooler and quieter, much slower
};

const char* gpu_choice_name(GpuChoice choice);  // "auto", "dedicated", "integrated"
std::optional<GpuChoice> gpu_choice_from_name(const std::string& name);

// Preferences of this user on this machine (not of a game): ~/.config/redgem/settings.json.
struct UserSettings {
  GpuChoice gpu = GpuChoice::Auto;
};

std::filesystem::path user_settings_file();
// Missing or unreadable files give the defaults.
UserSettings read_user_settings(const std::filesystem::path& file);
bool write_user_settings(const std::filesystem::path& file, const UserSettings& settings, std::string& error);

// The card to use: what the command line asked for, else the REDGEM_GPU environment variable,
// else the saved setting, else Auto. Unknown names are ignored.
GpuChoice resolve_gpu_choice(const std::optional<std::string>& command_line,
                             const std::optional<std::string>& environment, const UserSettings& settings);

// Resolves the choice for this process and publishes it (as REDGEM_GPU) for the renderer,
// which reads it when it opens the window. Call before any window is created.
GpuChoice apply_gpu_choice(const std::optional<std::string>& command_line);

}  // namespace redgem
