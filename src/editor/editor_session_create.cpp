// Part of EditorSession: the objects the menus create, and what they start as.

#include "editor/editor_session.hpp"

#include <utility>

#include "core/log.hpp"
#include "editor/new_objects.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

// Defaults of objects created from the editor's menus.
constexpr const char* kNewEmptyName = "GameObject";
constexpr const char* kNewTerrainName = "Terrain";
constexpr const char* kNewEnvironmentName = "Environment";
constexpr float kNewTerrainSize = 100.0f;                // meters across
constexpr int kNewTerrainCells = 64;                     // quads per side: about 1.5 m each
constexpr float kGroundLevel = 0.0f;                     // meters: new terrains lie at y = 0
constexpr Color kNewShapeColor{0.8f, 0.8f, 0.8f, 1.0f};  // light gray
constexpr float kNewLampIntensity = 500.0f;              // watts, like Blender's default point light

// What GameObject > Environment starts with: a sunset (colors as "#rrggbb").
constexpr std::pair<const char*, const char*> kSunsetColors[] = {{props::environment::kSkyTop, "#33457f"},
                                                                 {props::environment::kSkyHorizon, "#ffab73"},
                                                                 {props::environment::kGroundColor, "#4a3c3a"},
                                                                 {props::environment::kFogColor, "#e8a988"}};
constexpr std::pair<const char*, double> kSunsetNumbers[] = {
    {props::environment::kFogDensity, 0.003}, {props::environment::kAmbient, 0.8},
    {props::environment::kExposure, 1.0},     {props::environment::kBloom, 0.45},
    {props::environment::kVignette, 0.35},    {props::environment::kSunGlow, 1.6},
    {props::environment::kWind, 0.6}};

const char* light_name(LightType type) {
  return type == LightType::Directional ? new_objects::kSunName
         : type == LightType::Spot      ? "Spot Light"
                                        : "Point Light";
}

}  // namespace

ObjectId EditorSession::place_new(int node, Vec3 position) {
  Transform local = document_.node(node).local;
  local.position = position;
  document_.set_local(node, local, false);  // part of the creation's undo step
  refresh_preview();
  selection_ = node;
  return node;
}

ObjectId EditorSession::place_created(const std::function<int()>& add, Vec3 position) {
  if (!editing()) return kNoObject;
  return place_new(add(), position);
}

ObjectId EditorSession::create_empty(Vec3 position) {
  return place_created([this] { return document_.add_empty(document_.unique_name(kNewEmptyName)); }, position);
}

ObjectId EditorSession::create_primitive(PrimitiveShape shape, Vec3 position) {
  const auto add = [this, shape] {
    return document_.add_primitive(shape, document_.unique_name(primitive_name(shape)), kNewShapeColor);
  };
  return place_created(add, position);
}

ObjectId EditorSession::create_terrain(Vec3 position) {
  const auto add = [this] {
    return document_.add_terrain(document_.unique_name(kNewTerrainName), {kNewTerrainSize, kNewTerrainCells},
                                 new_objects::kGrassColor);
  };
  return place_created(add, {position.x, kGroundLevel, position.z});  // wherever the camera looks from
}

// A new light, aimed like a sun unless it shines all around.
int EditorSession::add_light(LightType type) {
  const float intensity = type == LightType::Directional ? new_objects::kSunIntensity : kNewLampIntensity;
  const int node =
      document_.add_light(type, document_.unique_name(light_name(type)), new_objects::kLightColor, intensity);
  if (type != LightType::Point) {
    Transform local = document_.node(node).local;
    local.rotation = new_objects::sun_rotation();
    document_.set_local(node, local, false);
  }
  return node;
}

ObjectId EditorSession::create_light(LightType type, Vec3 position) {
  return place_created([this, type] { return add_light(type); }, position);
}

ObjectId EditorSession::create_camera(Vec3 position) {
  const auto add = [this] {
    return document_.add_camera(document_.unique_name(new_objects::kCameraName), new_objects::kCameraFov);
  };
  return place_created(add, position);
}

ObjectId EditorSession::find_environment() const {
  for (int i = 0; i < document_.node_count(); ++i) {
    if (get_bool(document_.node(i).props, props::kEnvironment).value_or(false)) return i;
  }
  return kNoObject;
}

ObjectId EditorSession::create_environment() {
  if (!editing()) return kNoObject;
  const ObjectId existing = find_environment();
  if (existing != kNoObject) {
    selection_ = existing;
    return existing;
  }
  const int node = document_.add_empty(document_.unique_name(kNewEnvironmentName));
  document_.set_prop(node, props::kEnvironment, true, false);  // same undo step as the creation
  for (const auto& [key, value] : kSunsetColors) document_.set_prop(node, key, std::string(value), false);
  for (const auto& [key, value] : kSunsetNumbers) document_.set_prop(node, key, value, false);
  return place_new(node, Vec3::zero());
}

ObjectId EditorSession::import_model(const std::filesystem::path& path, Vec3 position, std::string& error,
                                     ObjectId parent) {
  if (!editing()) {
    error = "stop the game to edit the scene";
    return kNoObject;
  }
  const int node = document_.import_model(path, error, parent);
  if (node == kNoIndex) return kNoObject;
  log::info("imported " + path.filename().string());
  if (parent == kNoObject) return place_new(node, position);
  refresh_preview();  // the parent's world placement is needed below
  Transform world = preview_.world_transform(node);
  world.position = position;
  return place_new(node, relative_to(preview_.world_transform(parent), world).position);
}

}  // namespace redgem
