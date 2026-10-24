#pragma once

// A scene as short text (`redgem scene list` and `get`): one line per object, indented by the
// hierarchy, saying only what differs from an object's defaults, so that reading a level costs
// little. Every object is named by its id, `#<n>`: its place in the scene file, which the editor
// saves parents first (adding or removing objects renumbers those after them).

#include <cstddef>
#include <limits>
#include <string>

#include "scene/scene_desc.hpp"

namespace redgem::tools {

// Decimals a list shows: positions and sizes to the centimeter, angles to a tenth of a degree.
inline constexpr int kListDecimals = 2;
inline constexpr int kListAngleDecimals = 1;
// What `get` shows of them.
inline constexpr int kDetailDecimals = 4;

// What the id of an object starts with.
inline constexpr char kIdMark = '#';

// "#12".
std::string object_id(int index);
// A number with at most `decimals` decimals and no trailing zeros: 1.50 -> "1.5", -0.001 -> "0".
std::string number_text(double value, int decimals);
// "x,y,z", each a number_text.
std::string vector_text(Vec3 value, int decimals);
// A property's value as `set` takes it back: true, 2.5, a text (in quotes when it has a space).
std::string property_text(const PropertyValue& value);
// Where an object is in the hierarchy: "Level/Walls/Wall".
std::string object_path(const SceneDesc& scene, int index);

// The id `index` gets once the scene is saved and read again: files keep each object's children
// right after it, in order, and the objects at the top in the order they are (edits made since the
// scene was read may have put them elsewhere).
int saved_id(const SceneDesc& scene, int index);

// One object's line, without its indentation: "#3 Coin pos=3,1,0 rot=0,45,0 mesh=Coin scripts=Coin tag=coin".
std::string object_line(const SceneDesc& scene, int index);

// A list that goes all the way down the hierarchy.
inline constexpr std::size_t kEveryLevel = std::numeric_limits<std::size_t>::max();

// Which part of a scene a list shows.
struct ListLimits {
  std::size_t depth = kEveryLevel;  // how deep under the top objects (0: those alone)
  std::size_t most = 0;             // how many lines at most (0: every one)
};
// Every object of the scene, a line each, children under their parent: a first line says what
// the scene holds, and a last one what the limits left out.
std::string scene_list(const SceneDesc& scene, const std::string& scene_name, const ListLimits& limits);

// Everything about one object, a line per part.
std::string object_detail(const SceneDesc& scene, int index);

}  // namespace redgem::tools
