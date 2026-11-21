#include "editor/new_object_kinds.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

#include "editor/new_objects.hpp"
#include "scene/property.hpp"
#include "scene/props.hpp"
#include "scene/shapes/primitives.hpp"

namespace redgem::new_objects {
namespace {

constexpr float kTerrainSize = 100.0f;                // meters across
constexpr int kTerrainCells = 64;                     // quads per side: about 1.5 m each
constexpr Color kShapeColor{0.8f, 0.8f, 0.8f, 1.0f};  // light gray
constexpr float kLampIntensity = 500.0f;              // watts, like Blender's default point light

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

// A kind, the word tools name it with, and what the menu calls a new one (shapes: their own name).
struct KindWord {
  Kind kind;
  const char* word;
  const char* name;
};
constexpr KindWord kKindWords[] = {
    {Kind::Empty, "empty", "GameObject"},
    {Kind::Cube, "cube", nullptr},
    {Kind::Sphere, "sphere", nullptr},
    {Kind::Plane, "plane", nullptr},
    {Kind::Cylinder, "cylinder", nullptr},
    {Kind::Capsule, "capsule", nullptr},
    {Kind::Sun, "sun", kSunName},
    {Kind::PointLight, "point_light", "Point Light"},
    {Kind::SpotLight, "spot_light", "Spot Light"},
    {Kind::Camera, "camera", kCameraName},
    {Kind::Terrain, "terrain", "Terrain"},
    {Kind::Environment, "environment", "Environment"},
};

struct ShapeKind {
  Kind kind;
  PrimitiveShape shape;
};
constexpr ShapeKind kShapes[] = {{Kind::Cube, PrimitiveShape::Cube},
                                 {Kind::Sphere, PrimitiveShape::Sphere},
                                 {Kind::Plane, PrimitiveShape::Plane},
                                 {Kind::Cylinder, PrimitiveShape::Cylinder},
                                 {Kind::Capsule, PrimitiveShape::Capsule}};

const ShapeKind* shape_of(Kind kind) {
  const auto* found = std::find_if(std::begin(kShapes), std::end(kShapes),
                                   [kind](const ShapeKind& shape) { return shape.kind == kind; });
  return found == std::end(kShapes) ? nullptr : found;
}

bool is_light(Kind kind) { return kind == Kind::Sun || kind == Kind::PointLight || kind == Kind::SpotLight; }

LightType light_type(Kind kind) {
  if (kind == Kind::Sun) return LightType::Directional;
  return kind == Kind::SpotLight ? LightType::Spot : LightType::Point;
}

// A new light, aimed like a sun unless it shines all around.
int add_light(SceneDocument& document, LightType type, const std::string& name, int parent) {
  const float intensity = type == LightType::Directional ? kSunIntensity : kLampIntensity;
  const int node = document.add_light(type, name, kLightColor, intensity, parent);
  if (type == LightType::Point) return node;
  Transform local = document.node(node).local;
  local.rotation = sun_rotation();
  document.set_local(node, local, false);  // part of the creation's undo step
  return node;
}

int add_environment(SceneDocument& document, const std::string& name, int parent) {
  const int node = document.add_empty(name, parent);
  document.set_prop(node, props::kEnvironment, true, false);  // same undo step as the creation
  for (const auto& [key, value] : kSunsetColors) document.set_prop(node, key, std::string(value), false);
  for (const auto& [key, value] : kSunsetNumbers) document.set_prop(node, key, value, false);
  return node;
}

// The kinds that are neither shapes nor lights.
int add_other(SceneDocument& document, Kind kind, const std::string& name, int parent) {
  if (kind == Kind::Camera) return document.add_camera(name, kCameraFov, parent);
  if (kind == Kind::Terrain) return document.add_terrain(name, {kTerrainSize, kTerrainCells}, kGrassColor, parent);
  if (kind == Kind::Environment) return add_environment(document, name, parent);
  return document.add_empty(name, parent);
}

}  // namespace

std::optional<Kind> kind_named(const std::string& word) {
  const auto* found = std::find_if(std::begin(kKindWords), std::end(kKindWords),
                                   [&word](const KindWord& known) { return word == known.word; });
  if (found == std::end(kKindWords)) return std::nullopt;
  return found->kind;
}

std::string kind_words() {
  std::string words;
  for (const KindWord& known : kKindWords) words += (words.empty() ? "" : ", ") + std::string(known.word);
  return words;
}

std::string default_name(Kind kind) {
  if (const ShapeKind* shape = shape_of(kind)) return primitive_name(shape->shape);
  const auto* found = std::find_if(std::begin(kKindWords), std::end(kKindWords),
                                   [kind](const KindWord& known) { return known.kind == kind; });
  return found->name;
}

int add_object(SceneDocument& document, Kind kind, const std::string& name, int parent) {
  const std::string unique = document.unique_name(name);
  if (const ShapeKind* shape = shape_of(kind)) return document.add_primitive(shape->shape, unique, kShapeColor, parent);
  if (is_light(kind)) return add_light(document, light_type(kind), unique, parent);
  return add_other(document, kind, unique, parent);
}

int environment_of(const SceneDocument& document) {
  for (int i = 0; i < document.node_count(); ++i) {
    if (get_bool(document.node(i).props, props::kEnvironment).value_or(false)) return i;
  }
  return kNoIndex;
}

}  // namespace redgem::new_objects
