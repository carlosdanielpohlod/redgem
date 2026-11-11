#include "render/scene_lights.hpp"

#include <algorithm>
#include <cmath>

#include "render/render_config.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

constexpr float kRightAngle = kPi * kHalf;  // radians: the widest a spot's cone opens off its axis
// The least the cosines of where a spot starts dimming and where it is gone differ (glTF's): a
// cone with no dimming edge still gets a narrow one instead of dividing by zero.
constexpr float kNarrowestEdge = 0.001f;

// The sun scenes without a directional light get.
SceneLights default_lights() {
  SceneLights lights;
  lights.sun_direction =
      normalize(Vec3{config::kDefaultSunDirectionX, config::kDefaultSunDirectionY, config::kDefaultSunDirectionZ});
  lights.sun_color = Vec3::one() * config::kDefaultSunIntensity;
  return lights;
}

Vec3 rgb(const Color& color) { return {color.r, color.g, color.b}; }

// A spot shines along its object's forward (-Z), within its cone.
void make_spot(PointLight& out, const LightDesc& light, Vec3 forward) {
  out.spot = true;
  out.axis = normalize(forward);
  out.cone = spot_cone(light.inner_cone, light.outer_cone);
  out.outer_cone = std::clamp(light.outer_cone, 0.0f, kRightAngle);
}

// A point or spot light where its object is. It casts shadows only when its object says so.
PointLight local_light(const LightDesc& light, const Transform& world, const PropertyMap& props) {
  PointLight out{world.position, rgb(light.color) * (light.intensity * config::kPointWattScale),
                 light.range > 0.0f ? light.range : config::kDefaultPointLightRange};
  out.cast_shadow = get_bool(props, props::kCastShadow).value_or(false);
  if (light.type == LightType::Spot) make_spot(out, light, world.forward());
  return out;
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

// A light of the scene: one more point or spot light, or the sun (the first directional light;
// the others are ignored). `props`: its object's.
void add_light(SceneLights& lights, const LightDesc& light, const Transform& world, const PropertyMap& props,
               bool& sun_found) {
  if (light.type != LightType::Directional) {
    lights.points.push_back(local_light(light, world, props));
    return;
  }
  if (sun_found) return;
  sun_found = true;
  lights.sun_direction = world.forward();
  lights.sun_color = rgb(light.color) * (light.intensity * config::kSunStrengthScale);
}

}  // namespace

SpotCone spot_cone(float inner, float outer) {
  const float widest = std::clamp(outer, 0.0f, kRightAngle);
  const float start = std::clamp(inner, 0.0f, widest);
  const float scale = 1.0f / std::max(std::cos(start) - std::cos(widest), kNarrowestEdge);
  return {scale, -std::cos(widest) * scale};
}

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
    const GameObject& object = scene.at(id);
    const LightDesc* light = shining_light(object);
    if (light != nullptr) add_light(lights, *light, scene.world_transform(id), object.props, sun_found);
  }
  return lights;
}

}  // namespace redgem
