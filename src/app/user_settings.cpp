#include "app/user_settings.hpp"

#include "core/platform.hpp"

#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/files.hpp"

namespace redgem {
namespace {

constexpr const char* kSettingsFileName = "settings.json";
constexpr const char* kGpuKey = "gpu";
constexpr const char* kGpuVariable = "REDGEM_GPU";  // the user's choice of card for what is started from a shell
constexpr int kJsonIndent = 2;

}  // namespace

std::filesystem::path user_settings_file() { return platform::user_data_dir() / kSettingsFileName; }

UserSettings read_user_settings(const std::filesystem::path& file) {
  UserSettings settings;
  std::ifstream in(file);
  if (!in) return settings;
  const nlohmann::json json = nlohmann::json::parse(in, nullptr, false);
  if (!json.is_object()) return settings;
  if (json.contains(kGpuKey) && json[kGpuKey].is_string()) {
    settings.gpu = gpu_choice_from_name(json[kGpuKey].get<std::string>()).value_or(settings.gpu);
  }
  return settings;
}

bool write_user_settings(const std::filesystem::path& file, const UserSettings& settings, std::string& error) {
  std::error_code fs_error;
  std::filesystem::create_directories(file.parent_path(), fs_error);
  const nlohmann::json json = {{kGpuKey, gpu_choice_name(settings.gpu)}};
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
  const char* variable = std::getenv(kGpuVariable);
  const std::optional<std::string> environment =
      variable != nullptr ? std::optional<std::string>(variable) : std::nullopt;
  return resolve_gpu_choice(command_line, environment, read_user_settings(user_settings_file()));
}

}  // namespace redgem
