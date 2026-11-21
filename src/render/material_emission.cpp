#include "render/material_emission.hpp"

#include <algorithm>

namespace redgem {

namespace {

bool same_light(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

bool same_entry(const EmissionOverrides::value_type& a, const EmissionOverrides::value_type& b) {
  return a.first == b.first && same_light(a.second, b.second);
}

}  // namespace

bool same_emission(const EmissionOverrides& a, const EmissionOverrides& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), same_entry);
}

std::vector<MaterialDesc> with_emission(const std::vector<MaterialDesc>& loaded, const EmissionOverrides& overrides) {
  std::vector<MaterialDesc> drawn = loaded;
  if (overrides.empty()) return drawn;
  for (MaterialDesc& material : drawn) {
    const auto found = overrides.find(material.name);
    if (found != overrides.end()) material.emissive = found->second;
  }
  return drawn;
}

}  // namespace redgem
