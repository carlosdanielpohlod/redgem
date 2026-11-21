#pragma once

// What scripts change of the materials' glow while the game runs (Material.set_emission), by
// material name: lit windows at night, a lamp switched on. The renderer lays it over the glow the
// scene's file gave each material of that name.

#include <map>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// Material name -> emitted light, linear RGB already multiplied by its strength.
using EmissionOverrides = std::map<std::string, Vec3>;

// Whether two sets of overrides light the same materials the same.
bool same_emission(const EmissionOverrides& a, const EmissionOverrides& b);

// The materials as drawn: `loaded` (as the scene's file has them), with the emission of every one
// whose name is in `overrides` replaced. Both lists are the same length.
std::vector<MaterialDesc> with_emission(const std::vector<MaterialDesc>& loaded, const EmissionOverrides& overrides);

}  // namespace redgem
