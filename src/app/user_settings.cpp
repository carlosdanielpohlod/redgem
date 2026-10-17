#include "app/user_settings.hpp"

#include "core/platform.hpp"

#include <nlohmann/json.hpp>

#include "app/json_file.hpp"
#include "core/files.hpp"

namespace redgem {
namespace {

constexpr const char* kSettingsFileName = "settings.json";
constexpr const char* kGpuKey = "gpu";
constexpr const char* kLanguageKey = "language";
constexpr const char* kGpuVariable = "REDGEM_GPU";  // the user's choice of card for what is started from a shell
constexpr int kJsonIndent = 2;

}  // namespace

std::filesystem::path user_settings_file() { return platform::user_data_dir() / kSettingsFileName; }

UserSettings read_user_settings(const std::filesystem::path& file) {
  UserSettings settings;
  const nlohmann::json json = read_json_file(file);
  if (holds_text(json, kGpuKey)) {
    settings.gpu = gpu_choice_from_name(json[kGpuKey].get<std::string>()).value_or(settings.gpu);
  }
  if (holds_text(json, kLanguageKey)) settings.language = json[kLanguageKey].get<std::string>();
  return settings;
}

bool write_user_settings(const std::filesystem::path& file, const UserSettings& settings, std::string& error) {
  std::error_code fs_error;
  std::filesystem::create_directories(file.parent_path(), fs_error);
  nlohmann::json json = {{kGpuKey, gpu_choice_name(settings.gpu)}};
  if (!settings.language.empty()) json[kLanguageKey] = settings.language;
  return write_text_file(file, json.dump(kJsonIndent) + "\n", error);
}

GpuChoice resolve_gpu_choice(const std::optional<std::string>& command_line,
                             const std::optional<std::string>& environment, const UserSettings& settings) {
  for (const std::optional<std::string>* source : {&command_line, &environment}) {
    if (!source->has_value()) continue;
    if (const auto choice = gpu_choice_from_name(**source)) return *choice;
  }
  return settings.gpu;
}

GpuChoice gpu_choice_for_run(const std::optional<std::string>& command_line) {
  const std::string variable = platform::environment(kGpuVariable);
  const std::optional<std::string> environment = variable.empty() ? std::nullopt : std::optional(variable);
  return resolve_gpu_choice(command_line, environment, read_user_settings(user_settings_file()));
}

std::string language_for_run() {
  const std::string chosen = read_user_settings(user_settings_file()).language;
  return chosen.empty() ? platform::system_language() : chosen;
}

}  // namespace redgem
