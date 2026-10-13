#include "render/scene_lights.hpp"

#include <algorithm>

#include "render/render_config.hpp"

namespace redgem {
namespace {

// The sun scenes without a directional light get.
SceneLights default_lights() {
  SceneLights lights;
  lights.sun_direction =
      normalize(Vec3{config::kDefaultSunDirectionX, config::kDefaultSunDirectionY, config::kDefaultSunDirectionZ});
  lights.sun_color = Vec3::one() * config::kDefaultSunIntensity;
  return lights;
}

Vec3 rgb(const Color& color) { return {color.r, color.g, color.b}; }

PointLight point_light(const LightDesc& light, Vec3 position) {
  return {position, rgb(light.color) * (light.intensity * config::kPointWattScale),
          light.range > 0.0f ? light.range : config::kDefaultPointLightRange};
}

// How far a light's reach ends from the viewer: a far but wide light can matter more than a near
// but tiny one.
float reach(const PointLight& light, Vec3 viewer) { return length(light.position - viewer) - light.range; }

// Lights fade to nothing as their reach approaches `cut`, over `fade_distance` meters.
void fade_towards(std::vector<PointLight>& lights, Vec3 viewer, float cut, float fade_distance) {
  for (PointLight& light : lights) {
    light.color = light.color * std::clamp((cut - reach(light, viewer)) / fade_distance, 0.0f, 1.0f);
  }
}

// The light an object shines with, or null. (The lights of prefabs do not shine until they are
// spawned.)
const LightDesc* shining_light(const GameObject& object) {
  if (object.prefab) return nullptr;
  return object.light ? &*object.light : nullptr;
}

// A light of the scene: one more point light, or the sun (the first directional light; the
// others are ignored).
void add_light(SceneLights& lights, const LightDesc& light, const Transform& world, bool& sun_found) {
  if (light.type != LightType::Directional) {
    lights.points.push_back(point_light(light, world.position));
    return;
  }
  if (sun_found) return;
  sun_found = true;
  lights.sun_direction = world.forward();
  lights.sun_color = rgb(light.color) * (light.intensity * config::kSunStrengthScale);
}

}  // namespace

std::vector<PointLight> nearest_lights(std::vector<PointLight> lights, Vec3 viewer, std::size_t max_count,
                                       float fade_distance) {
  std::stable_sort(lights.begin(), lights.end(),
                   [viewer](const PointLight& a, const PointLight& b) { return reach(a, viewer) < reach(b, viewer); });
  if (lights.size() <= max_count) return lights;
  // The first light left out marks the cut: kept lights fade to nothing as they approach it.
  const float cut = reach(lights[max_count], viewer);
  lights.resize(max_count);
  if (fade_distance > 0.0f) fade_towards(lights, viewer, cut, fade_distance);
  return lights;
}

SceneLights scene_lights(const Scene& scene) {
  SceneLights lights = default_lights();
  bool sun_found = false;
  for (const ObjectId id : scene.alive_ids()) {
    const LightDesc* light = shining_light(scene.at(id));
    if (light != nullptr) add_light(lights, *light, scene.world_transform(id), sun_found);
  }
  return lights;
}

}  // namespace redgem
