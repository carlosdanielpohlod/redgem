#include "app/prefab_budget.hpp"

#include <algorithm>

namespace redgem {
namespace {

constexpr const char* kSourceKey = "source";
constexpr const char* kDecodedKey = "decoded";

bool known_key(const std::string& key) { return key == kSourceKey || key == kDecodedKey; }

// Any whole number from 1, however large (a huge one is clamped); negative ones are signed.
bool whole_megabytes(const nlohmann::json& value) {
  return value.is_number_unsigned() && value.get<std::uint64_t>() >= 1;
}

bool usable_entry(const std::string& key, const nlohmann::json& value) {
  return known_key(key) && whole_megabytes(value);
}

bool usable_section(const nlohmann::json& section) {
  if (!section.is_object()) return false;
  return std::all_of(section.items().begin(), section.items().end(),
                     [](const auto& entry) { return usable_entry(entry.key(), entry.value()); });
}

// Mebibytes as bytes, at most a scene's own budget.
std::uint64_t clamped_bytes(const nlohmann::json& megabytes) {
  const auto asked = std::min(megabytes.get<std::uint64_t>(), prefabs::kMostBudgetMb);
  return asked * prefabs::Limits::kMebibyte;
}

void take(const nlohmann::json& section, const char* key, std::uint64_t& bytes) {
  if (section.contains(key)) bytes = clamped_bytes(section[key]);
}

std::string problem() {
  return std::string("game.json: ") + prefabs::kBudgetKey + " must be {\"" + kSourceKey + "\": MiB, \"" + kDecodedKey +
         "\": MiB}, each a whole number from 1 (at most " + std::to_string(prefabs::kMostBudgetMb) +
         " is used); the prefabs get the engine's budgets";
}

}  // namespace

std::string read_prefab_budget(const nlohmann::json& json, PrefabBudget& budget) {
  if (!json.contains(prefabs::kBudgetKey)) return {};
  const auto& section = json[prefabs::kBudgetKey];
  if (!usable_section(section)) return problem();
  take(section, kSourceKey, budget.source_bytes);
  take(section, kDecodedKey, budget.decoded_bytes);
  return {};
}

prefabs::Limits prefab_limits(const PrefabBudget& budget) {
  prefabs::Limits limits;
  limits.input_bytes = budget.source_bytes;
  limits.expanded_bytes = budget.decoded_bytes;
  return limits;
}

}  // namespace redgem
