#include "editor/editor_logic.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "editor/editor_config.hpp"

namespace redgem::editor_logic {
namespace {

using namespace editor_config;

constexpr const char* kTitleSeparator = " - ";
constexpr const char* kUnsavedMark = " *";
constexpr const char* kPlayingMark = " [playing]";
constexpr const char* kTargetSeparator = "-";
constexpr const char* kHomeShorthand = "~";  // what a path under the home folder starts with
constexpr const char* kHomePrefix = "~/";
constexpr int kCornerBitX = 1;  // bits of a corner's number: set = the box's maximum on that axis
constexpr int kCornerBitY = 2;
constexpr int kCornerBitZ = 4;
constexpr float kCenter = 0.5f;                // halfway
constexpr float kBothSides = 2.0f;             // padding on the two sides of a text
constexpr float kMaxDropDistance = 200.0f;     // meters: a drop further away than this counts as the sky
constexpr float kFlyAxisFull = 1.0f;           // a held key pushes its axis all the way
constexpr float kSmoothingPerStrength = 0.5f;  // Smooth and Flatten: share of the way per second, per unit of strength

bool starts_with(const std::string& text, const std::string& prefix) { return text.rfind(prefix, 0) == 0; }

}  // namespace

std::string window_title(const std::string& game, bool unsaved, bool playing) {
  std::string title = std::string(kWindowTitle) + kTitleSeparator + game;
  if (unsaved) title += kUnsavedMark;
  if (playing) title += kPlayingMark;
  return title;
}

std::string build_folder_name(const std::string& game, const std::string& target, bool is_host_target) {
  const std::string name = game + kBuildSuffix;
  return is_host_target ? name : name + kTargetSeparator + target;
}

std::string pretty_path(const std::string& path, const std::string& home) {
  if (!home.empty() && starts_with(path, home)) return kHomeShorthand + path.substr(home.size());
  return path;
}

std::filesystem::path expand_home(const std::filesystem::path& typed, const std::filesystem::path& home) {
  const std::string text = typed.string();
  const std::string prefix = kHomePrefix;
  return starts_with(text, prefix) ? home / text.substr(prefix.size()) : typed;
}

std::string capitalized(std::string name) {
  name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
  return name;
}

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
  return text;
}

bool name_contains(const std::string& name, const std::string& wanted) {
  return lowercase(name).find(wanted) != std::string::npos;
}

float centered_start(float available, float size) { return (available - size) * kCenter; }

Rect hint_pill(float view_x, float view_y, float view_height, float text_width, float text_height) {
  Rect pill;
  pill.min_x = view_x + kOverlayMargin;
  pill.min_y = view_y + view_height - kOverlayMargin - text_height - kOverlayPadding * kBothSides;
  pill.max_x = pill.min_x + text_width + kOverlayPadding * kBothSides;
  pill.max_y = pill.min_y + text_height + kOverlayPadding * kBothSides;
  return pill;
}

int tile_columns(float available_width, float spacing, float tile_width) {
  return std::max(1, static_cast<int>((available_width + spacing) / (tile_width + spacing)));
}

float inspector_split_ratio() { return kInspectorWidth / (1.0f - kHierarchyWidth); }

int index_of(const char* const* names, int count, const std::string& value, int fallback) {
  for (int i = 0; i < count; ++i) {
    if (value == names[i]) return i;
  }
  return fallback;
}

std::vector<int> materials_of(const MeshDesc& mesh) {
  std::vector<int> materials;
  for (const PrimitiveDesc& primitive : mesh.primitives) {
    const bool listed = std::find(materials.begin(), materials.end(), primitive.material) != materials.end();
    if (primitive.material != kNoIndex && !listed) materials.push_back(primitive.material);
  }
  return materials;
}

std::array<Vec3, kBoxCorners> box_corners(const Transform& world, const Bounds& bounds) {
  std::array<Vec3, kBoxCorners> corners;
  for (int i = 0; i < kBoxCorners; ++i) {
    const Vec3 local{(i & kCornerBitX) ? bounds.max.x : bounds.min.x, (i & kCornerBitY) ? bounds.max.y : bounds.min.y,
                     (i & kCornerBitZ) ? bounds.max.z : bounds.min.z};
    corners[i] = world.position + rotate(world.rotation, hadamard(local, world.scale));
  }
  return corners;
}

Vec3 drop_point(Vec3 ray_origin, Vec3 ray_direction, Vec3 fallback) {
  if (!(ray_direction.y < 0.0f)) return fallback;
  const float distance = -ray_origin.y / ray_direction.y;
  return distance < kMaxDropDistance ? ray_origin + ray_direction * distance : fallback;
}

float fly_axis(bool negative_down, bool positive_down) {
  return (positive_down ? kFlyAxisFull : 0.0f) - (negative_down ? kFlyAxisFull : 0.0f);
}

std::vector<std::string> without_item(std::vector<std::string> items, std::size_t index) {
  items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
  return items;
}

Vec3 at_least(Vec3 value, float lowest) {
  return {std::max(value.x, lowest), std::max(value.y, lowest), std::max(value.z, lowest)};
}

SculptMode reversed(SculptMode mode) {
  if (mode == SculptMode::Raise) return SculptMode::Lower;
  return mode == SculptMode::Lower ? SculptMode::Raise : mode;
}

bool moves_ground(SculptMode mode) { return mode == SculptMode::Raise || mode == SculptMode::Lower; }

float brush_amount(SculptMode mode, float strength, float seconds) {
  return strength * seconds * (moves_ground(mode) ? 1.0f : kSmoothingPerStrength);
}

Vec3 to_local(const Transform& world, Vec3 point) {
  const Vec3 offset = rotate(conjugate(world.rotation), point - world.position);
  return {offset.x / world.scale.x, offset.y / world.scale.y, offset.z / world.scale.z};
}

Vec3 to_world(const Transform& world, Vec3 point) {
  return world.position + rotate(world.rotation, hadamard(point, world.scale));
}

TerrainBrush local_brush(const Transform& terrain_world, Vec3 world_point, float radius, float amount,
                         SculptMode mode) {
  const Vec3 local = to_local(terrain_world, world_point);
  return {local.x, local.z, radius / std::fabs(terrain_world.scale.x),
          moves_ground(mode) ? amount / std::fabs(terrain_world.scale.y) : amount, mode};
}

}  // namespace redgem::editor_logic
