#include "script/npc_settings.hpp"

#include <algorithm>
#include <iterator>

namespace redgem {
namespace {

constexpr char kEntrySeparator = ',';  // as Schedule::ENTRY_SEPARATOR
constexpr const char* kBlanks = " \t\r\n";

// The settings of each part, as properties.
constexpr const char* kInteractableKeys[] = {"prompt", "range"};
constexpr const char* kPerceptionKeys[] = {"sight", "fov", "eyes", "hearing"};
constexpr const char* kPlanKey = "plan";

template <std::size_t N>
bool has_any(const PropertyMap& props, const char* const (&keys)[N]) {
  return std::any_of(std::begin(keys), std::end(keys), [&props](const char* key) { return props.count(key) != 0; });
}

float number_or(const PropertyMap& props, const char* key, float fallback) {
  const std::optional<double> number = get_number(props, key);
  return number ? static_cast<float>(*number) : fallback;
}

InteractableSettings interactable_from(const PropertyMap& props) {
  InteractableSettings settings;
  settings.range = number_or(props, "range", kInteractableRange);
  settings.prompt = get_string(props, "prompt");
  return settings;
}

PerceptionSettings perception_from(const PropertyMap& props) {
  PerceptionSettings settings;
  settings.sight = number_or(props, "sight", kPerceptionSight);
  settings.fov = number_or(props, "fov", kPerceptionFov);
  settings.eyes = number_or(props, "eyes", kPerceptionEyes);
  settings.hearing = number_or(props, "hearing", kPerceptionHearing);
  return settings;
}

std::string trimmed(const std::string& text) {
  const auto first = text.find_first_not_of(kBlanks);
  if (first == std::string::npos) return {};
  return text.substr(first, text.find_last_not_of(kBlanks) - first + 1);
}

}  // namespace

std::vector<std::string> schedule_entries(const std::string& plan) {
  std::vector<std::string> entries;
  std::size_t start = 0;
  while (start <= plan.size()) {
    const std::size_t end = std::min(plan.find(kEntrySeparator, start), plan.size());
    const std::string entry = trimmed(plan.substr(start, end - start));
    if (!entry.empty()) entries.push_back(entry);
    start = end + 1;
  }
  return entries;
}

NpcSettings npc_settings_from_props(const PropertyMap& props) {
  NpcSettings settings;
  if (has_any(props, kInteractableKeys)) settings.interactable = interactable_from(props);
  if (has_any(props, kPerceptionKeys)) settings.perception = perception_from(props);
  if (const auto plan = get_string(props, kPlanKey)) settings.schedule = schedule_entries(*plan);
  return settings;
}

}  // namespace redgem
