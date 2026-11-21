#include <doctest/doctest.h>

#include <string>

#include <nlohmann/json.hpp>

#include "app/prefab_budget.hpp"
#include "scene/gltf/gltf_budget.hpp"

using namespace redgem;

namespace {

constexpr std::uint64_t kMebibyte = prefabs::Limits::kMebibyte;
constexpr std::uint64_t kRaisedSourceMb = 1024;
constexpr std::uint64_t kRaisedDecodedMb = 2048;

PrefabBudget budget_of(const std::string& text, std::string& error) {
  PrefabBudget budget;
  error = read_prefab_budget(nlohmann::json::parse(text), budget);
  return budget;
}

bool defaults(const PrefabBudget& budget) {
  return budget.source_bytes == prefabs::Limits::kDefaultInputBytes &&
         budget.decoded_bytes == prefabs::Limits::kDefaultExpandedBytes;
}

}  // namespace

TEST_CASE("a game that says nothing keeps the engine's prefab budgets") {
  std::string error;
  const PrefabBudget budget = budget_of(R"({"title": "Island"})", error);
  CHECK(error.empty());
  CHECK(defaults(budget));
  const prefabs::Limits limits = prefab_limits(budget);
  CHECK(limits.input_bytes == prefabs::Limits::kDefaultInputBytes);
  CHECK(limits.expanded_bytes == prefabs::Limits::kDefaultExpandedBytes);
}

TEST_CASE("a large world raises its prefab budgets, one or both") {
  std::string error;
  const PrefabBudget both = budget_of(R"({"prefab_budget_mb": {"source": 1024, "decoded": 2048}})", error);
  CHECK(error.empty());
  CHECK(both.source_bytes == kRaisedSourceMb * kMebibyte);
  CHECK(both.decoded_bytes == kRaisedDecodedMb * kMebibyte);
  CHECK(prefab_limits(both).input_bytes == kRaisedSourceMb * kMebibyte);
  const PrefabBudget one = budget_of(R"({"prefab_budget_mb": {"decoded": 2048}})", error);
  CHECK(one.source_bytes == prefabs::Limits::kDefaultInputBytes);
  CHECK(one.decoded_bytes == kRaisedDecodedMb * kMebibyte);
}

TEST_CASE("a prefab budget stops at the scene's own byte budget") {
  std::string error;
  const PrefabBudget budget = budget_of(R"({"prefab_budget_mb": {"source": 100000, "decoded": 999999}})", error);
  CHECK(error.empty());
  CHECK(budget.source_bytes == gltf_budget::kMaxSceneBytes);
  CHECK(budget.decoded_bytes == gltf_budget::kMaxSceneBytes);
  const PrefabBudget huge = budget_of(R"({"prefab_budget_mb": {"source": 18446744073709551615}})", error);
  CHECK(error.empty());
  CHECK(huge.source_bytes == gltf_budget::kMaxSceneBytes);
}

TEST_CASE("a prefab budget that cannot be used is refused whole and the defaults stay") {
  const char* bad[] = {
      R"({"prefab_budget_mb": 1024})",
      R"({"prefab_budget_mb": {"source": 0}})",
      R"({"prefab_budget_mb": {"source": -5, "decoded": 2048}})",
      R"({"prefab_budget_mb": {"source": "big"}})",
      R"({"prefab_budget_mb": {"source": 1.5}})",
      R"({"prefab_budget_mb": {"sources": 1024}})",
  };
  for (const char* text : bad) {
    CAPTURE(text);
    std::string error;
    const PrefabBudget budget = budget_of(text, error);
    CHECK(error.find(prefabs::kBudgetKey) != std::string::npos);
    CHECK(defaults(budget));
  }
}
