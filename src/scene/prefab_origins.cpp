#include "scene/prefab_import.hpp"

namespace redgem::prefabs {
namespace {
void overlay(PropertyMap& target, const PropertyMap& values) {
  for (const auto& entry : values) {
    if (entry.first != kFileProperty) target.insert_or_assign(entry.first, entry.second);
  }
}
}  // namespace
void record_origin(Origins& origins, int id, const NodeDesc& authored, const NodeDesc& source) {
  auto [entry, inserted] = origins.try_emplace(id);
  Origin& origin = entry->second;
  if (inserted) {
    origin.reference = get_string(authored.props, kFileProperty).value_or("");
    origin.name = source.name;
  }
  PropertyMap defaults = source.props;
  defaults.erase(props::kPrefab);
  defaults.erase(kFileProperty);
  overlay(defaults, origin.defaults);
  origin.defaults = std::move(defaults);
  origin.roots.push_back(source.local);
}
Transform inherited_local(const Origin& origin, Transform placement) {
  for (const auto& local : origin.roots) placement = combine(placement, local);
  return checked_placement(placement);
}
PropertyMap inherited_props(const Origin& origin, const PropertyMap& authored) {
  PropertyMap result = origin.defaults;
  overlay(result, authored);
  return result;
}
}  // namespace redgem::prefabs
