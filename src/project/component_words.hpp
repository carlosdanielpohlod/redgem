#pragma once

// The words of `redgem scene set` (and of MCP scene_edit) that change an object's light or camera,
// as the Inspector's Light and Camera cards do:
//
//   light=sun|point|spot     the kind of an object's light (the object must have one)
//   light_color=#rrggbb      or r,g,b, each 0 to 1
//   light_intensity=W        watts for point and spot lights, W/m2 for the sun; 0 or more
//   light_range=m            how far a point or spot light reaches; 0 lets the engine choose
//   camera_fov=degrees       the vertical field of view, 5 to 150
//   camera_near=m            the near clipping distance, at least 0.001
//   camera_far=m             the far clipping distance; 0 is no far plane
//
// Each takes one object and one value, and gives back what is wrong (empty when it was applied).

#include <string>

#include "editor/document/scene_document.hpp"

namespace redgem::tools::component_words {

std::string set_light_type(SceneDocument& document, int index, const std::string& value);
std::string set_light_color(SceneDocument& document, int index, const std::string& value);
std::string set_light_intensity(SceneDocument& document, int index, const std::string& value);
std::string set_light_range(SceneDocument& document, int index, const std::string& value);
std::string set_camera_fov(SceneDocument& document, int index, const std::string& value);
std::string set_camera_near(SceneDocument& document, int index, const std::string& value);
std::string set_camera_far(SceneDocument& document, int index, const std::string& value);

}  // namespace redgem::tools::component_words
