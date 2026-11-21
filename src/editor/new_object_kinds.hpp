#pragma once

// The objects the GameObject menu creates, by kind, as the editor and the tools make them: a shape
// with its collider, a light aimed like a sun, a camera, a terrain, the environment.

#include <optional>
#include <string>

#include "core/math.hpp"
#include "editor/document/scene_document.hpp"

namespace redgem::new_objects {

enum class Kind {
  Empty,
  Cube,
  Sphere,
  Plane,
  Cylinder,
  Capsule,
  Sun,
  PointLight,
  SpotLight,
  Camera,
  Terrain,
  Environment
};

// The kind a tool names ("cube", "point_light"); nothing for a word that is no kind.
std::optional<Kind> kind_named(const std::string& word);
// Every kind's word, comma separated, for the message that says what a kind can be.
std::string kind_words();
// What the menu calls a new object of that kind ("Cube", "Point Light").
std::string default_name(Kind kind);

// Adds an object of that kind under `parent` (kNoIndex: the top of the scene), named `name` or,
// when another object has that name, a numbered one ("Cube.001"). It records its own undo step.
int add_object(SceneDocument& document, Kind kind, const std::string& name, int parent = kNoIndex);

// Where new terrains lie: at y = 0, wherever the camera looks from.
inline constexpr float kGroundLevel = 0.0f;
// The object with environment = true, or kNoIndex.
int environment_of(const SceneDocument& document);

}  // namespace redgem::new_objects
