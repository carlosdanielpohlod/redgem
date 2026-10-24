#include "app/script_budget_setting.hpp"

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "app/game_settings.hpp"
#include "app/json_file.hpp"
#include "core/log.hpp"
#include "script/script_config.hpp"

namespace redgem {
namespace {

bool says_budget(const nlohmann::json& json) { return json.is_object() && json.contains(config::kScriptBudgetKey); }

bool within_budget_range(std::int64_t asked) { return asked > 0 && asked <= config::kMaxScriptBudgetMs; }

bool holds_usable_budget(const nlohmann::json& json) {
  return holds_whole_number(json, config::kScriptBudgetKey) &&
         within_budget_range(json[config::kScriptBudgetKey].get<std::int64_t>());
}

void warn_unusable_budget() {
  log::warning(std::string(kGameSettingsFile) + ": " + config::kScriptBudgetKey + " must be a whole number from 1 to " +
               std::to_string(config::kMaxScriptBudgetMs) + "; the scripts get " +
               std::to_string(config::kDefaultScriptBudgetMs) + " ms");
}

}  // namespace

std::chrono::milliseconds script_budget_for(const std::filesystem::path& game_dir) {
  constexpr std::chrono::milliseconds kDefault(config::kDefaultScriptBudgetMs);
  const nlohmann::json json = read_json_file(game_dir / kGameSettingsFile);
  if (!says_budget(json)) return kDefault;
  if (holds_usable_budget(json)) return std::chrono::milliseconds(json[config::kScriptBudgetKey].get<int>());
  warn_unusable_budget();
  return kDefault;
}

}  // namespace redgem
