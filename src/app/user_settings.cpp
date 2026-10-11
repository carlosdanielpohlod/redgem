#include "app/user_settings.hpp"

#include "core/platform.hpp"

#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>

#include "app/game_project.hpp"
#include "editor/script_files.hpp"

namespace redgem {
namespace {

constexpr const char* kSettingsFileName = "settings.json";
constexpr const char* kGpuKey = "gpu";
constexpr const char* kGpuVariable = "REDGEM_GPU";
constexpr int kJsonIndent = 2;

struct NamedChoice {
  GpuChoice choice;
  const char* name;
};
constexpr NamedChoice kChoices[] = {
    {GpuChoice::Auto, "auto"}, {GpuChoice::Dedicated, "dedicated"}, {GpuChoice::Integrated, "integrated"}};

}  // namespace

const char* gpu_choice_name(GpuChoice choice) {
  for (const NamedChoice& named : kChoices) {
    if (named.choice == choice) return named.name;
  }
  return kChoices[0].name;
}

std::optional<GpuChoice> gpu_choice_from_name(const std::string& name) {
  for (const NamedChoice& named : kChoices) {
    if (name == named.name) return named.choice;
  }
  return std::nullopt;
}

std::filesystem::path user_settings_file() { return recent_games_file().parent_path() / kSettingsFileName; }

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

GpuChoice resolve_gpu_choice(const std::optional<std::string>& command_line, const std::optional<std::string>& environment,
                             const UserSettings& settings) {
  for (const std::optional<std::string>* source : {&command_line, &environment}) {
    if (!source->has_value()) continue;
    if (const auto choice = gpu_choice_from_name(**source)) return *choice;
  }
  return settings.gpu;
}

GpuChoice apply_gpu_choice(const std::optional<std::string>& command_line) {
  const char* variable = std::getenv(kGpuVariable);
  const std::optional<std::string> environment = variable != nullptr ? std::optional<std::string>(variable) : std::nullopt;
  const GpuChoice choice = resolve_gpu_choice(command_line, environment, read_user_settings(user_settings_file()));
  platform::set_environment(kGpuVariable, gpu_choice_name(choice), true);
  return choice;
}

}  // namespace redgem
