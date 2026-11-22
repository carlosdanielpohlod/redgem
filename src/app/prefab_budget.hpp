#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "scene/prefabs.hpp"

// How many bytes a scene's prefab files may take, as read from disk and once decoded: game.json's
//   "prefab_budget_mb": {"source": 1024, "decoded": 2048}
// A large world raises them. Each is at most prefabs::kMostBudgetMb (a scene's own budget): more
// is taken as that. Without the key, the engine's defaults.
namespace redgem {

struct PrefabBudget {
  std::uint64_t source_bytes = prefabs::Limits::kDefaultInputBytes;
  std::uint64_t decoded_bytes = prefabs::Limits::kDefaultExpandedBytes;
};

// Reads game.json's budget into `budget`; what is wrong with it otherwise (the budget unchanged).
std::string read_prefab_budget(const nlohmann::json& json, PrefabBudget& budget);
// The limits a scene's prefabs are expanded within.
prefabs::Limits prefab_limits(const PrefabBudget& budget);

}  // namespace redgem
