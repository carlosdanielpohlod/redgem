#include "physics/physics_threads.hpp"

#include <cstdint>

#include <nlohmann/json.hpp>

#include "physics/physics_config.hpp"

namespace redgem {
namespace {

constexpr const char* kSection = "game.json physics: ";

bool within_thread_range(std::int64_t asked) {
  return asked >= config::kDefaultPhysicsThreads && asked <= config::kMaxPhysicsThreads;
}

bool holds_usable_threads(const nlohmann::json& value) {
  return value.is_number_integer() && within_thread_range(value.get<std::int64_t>());
}

std::string unusable_threads() {
  return std::string(kSection) + "\"" + kPhysicsThreadsKey + "\" must be a whole number from " +
         std::to_string(config::kDefaultPhysicsThreads) + " to " + std::to_string(config::kMaxPhysicsThreads) +
         "; the physics is solved on " + std::to_string(config::kDefaultPhysicsThreads) + " thread";
}

}  // namespace

int physics_threads_from_json(const nlohmann::json& physics, std::vector<std::string>& warnings) {
  if (!physics.is_object() || !physics.contains(kPhysicsThreadsKey)) return config::kDefaultPhysicsThreads;
  const nlohmann::json& value = physics[kPhysicsThreadsKey];
  if (holds_usable_threads(value)) return value.get<int>();
  warnings.push_back(unusable_threads());
  return config::kDefaultPhysicsThreads;
}

void physics_threads_to_json(int threads, nlohmann::json& physics) {
  if (threads == config::kDefaultPhysicsThreads) {
    physics.erase(kPhysicsThreadsKey);
    return;
  }
  physics[kPhysicsThreadsKey] = config::bounded_physics_threads(threads);
}

}  // namespace redgem
