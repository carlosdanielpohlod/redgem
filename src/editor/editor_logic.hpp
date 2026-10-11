#pragma once

// What the editor decides without needing a window: names it shows, where things go in its
// layout, and the math of the Scene view. Kept apart from the drawing code so it can be tested.

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "scene/scene_desc.hpp"
#include "scene/terrain.hpp"
#include "scene/transform.hpp"

namespace redgem::editor_logic {

inline constexpr int kBoxCorners = 8;

// A rectangle of the window, in pixels.
struct Rect {
  float min_x = 0.0f;
  float min_y = 0.0f;
  float max_x = 0.0f;
  float max_y = 0.0f;
};

// --- Names -----------------------------------------------------------------------------------

// "RedGem Editor - <game>", with " *" for unsaved changes and " [playing]" while the game runs.
std::string window_title(const std::string& game, bool unsaved, bool playing);
// The folder Build fills: "<game>-build" for the system the editor runs on, or
// "<game>-build-<target>" for another one.
std::string build_folder_name(const std::string& game, const std::string& target, bool is_host_target);
// "~/RedGemGames/x" reads better than the full home path.
std::string pretty_path(const std::string& path, const std::string& home);
// "~/games/x" as typed in a path field, with the home folder written out.
std::filesystem::path expand_home(const std::filesystem::path& typed, const std::filesystem::path& home);
// "platformer" -> "Platformer".
std::string capitalized(std::string name);
std::string lowercase(std::string text);
// True when `name` contains `wanted` (already lowercase), whatever the case of the name.
bool name_contains(const std::string& name, const std::string& wanted);

// --- Layout ----------------------------------------------------------------------------------

// Where something `size` wide starts so that it sits in the middle of `available`.
float centered_start(float available, float size);
// The dark pill behind the Scene view's hint: in the view's bottom-left corner, around a text of
// `text_width` x `text_height`. The view starts at (view_x, view_y) and is `view_height` tall.
Rect hint_pill(float view_x, float view_y, float view_height, float text_width, float text_height);
// How many tiles `tile_width` wide (with `spacing` between them) fit in a row; at least one.
int tile_columns(float available_width, float spacing, float tile_width);
// Share of the space left of the Hierarchy that the Inspector takes, so that it ends up
// kInspectorWidth of the whole window.
float inspector_split_ratio();

// --- Lists -----------------------------------------------------------------------------------

// Position of `value` in `names`, or `fallback` when it is not one of them.
int index_of(const char* const* names, int count, const std::string& value, int fallback);
// The materials a mesh uses, each once, in the order its parts use them.
std::vector<int> materials_of(const MeshDesc& mesh);

// --- Scene view ------------------------------------------------------------------------------

// The corners of a box given in an object's local space, in the world. Corner i takes the box's
// maximum on x, y, z where bits 0, 1, 2 of i are set.
std::array<Vec3, kBoxCorners> box_corners(const Transform& world, const Bounds& bounds);
// Where something dropped on the Scene view lands: on the ground (y = 0) under the mouse ray, or
// at `fallback` when the ray aims at the sky or meets the ground too far away.
Vec3 drop_point(Vec3 ray_origin, Vec3 ray_direction, Vec3 fallback);
// One fly-camera axis from its two keys: -1, 0 or 1.
float fly_axis(bool negative_down, bool positive_down);

// --- Terrain brush ---------------------------------------------------------------------------

// Raise becomes Lower and Lower becomes Raise (holding Shift); the other brushes stay.
SculptMode reversed(SculptMode mode);
// True for the brushes whose amount is a height in meters (Raise, Lower).
bool moves_ground(SculptMode mode);
// How much a brush held for `seconds` does: meters for Raise and Lower, a share for the others.
float brush_amount(SculptMode mode, float strength, float seconds);
// A point of the world in an object's own space, and back.
Vec3 to_local(const Transform& world, Vec3 point);
Vec3 to_world(const Transform& world, Vec3 point);
// A brush given in the world (meters) as the terrain's own grid sees it, whatever its scale.
TerrainBrush local_brush(const Transform& terrain_world, Vec3 world_point, float radius, float amount, SculptMode mode);

}  // namespace redgem::editor_logic
