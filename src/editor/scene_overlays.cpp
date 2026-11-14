#include "editor/scene_overlays.hpp"

#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/joint_gizmo.hpp"
#include "physics/body_config.hpp"
#include "render/render_math.hpp"

namespace redgem {
namespace {

using namespace editor_config;
using render_math::to_raylib;

constexpr float kBrushLift = 0.05f;       // meters the brush ring floats above the ground
constexpr float kBrushPoleShare = 0.25f;  // height of the brush's center mark, as a share of its radius
constexpr Vector3 kWorldRight{1.0f, 0.0f, 0.0f};
constexpr float kQuarterTurn = 90.0f;  // degrees: lays a circle flat
constexpr int kBoxEdges = 12;          // of a wireframe box
constexpr int kEdgeEnds = 2;
// Which corners (see editor_logic::box_corners) each edge of a box joins.
constexpr int kBoxEdgeCorners[kBoxEdges][kEdgeEnds] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                                       {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

// Where each channel is in a color given as four numbers.
enum ColorChannel { kRed, kGreen, kBlue, kAlpha };

// Wireframe of a box given in an object's local space.
void draw_box(const Transform& world, const Bounds& bounds, ::Color color) {
  const auto corners = editor_logic::box_corners(world, bounds);
  for (const auto& edge : kBoxEdgeCorners) {
    DrawLine3D(to_raylib(corners[edge[0]]), to_raylib(corners[edge[1]]), color);
  }
}

// What a picture of the scene would not show of a camera, a light or an empty object: a small
// wire shape where it is, and for the first two a line along where it points.
void draw_camera_mark(const Transform& world) {
  DrawCubeWires(to_raylib(world.position), kIconSize, kIconSize, kIconSize, rgba(kIconColor));
  DrawLine3D(to_raylib(world.position), to_raylib(world.position + world.forward() * kCameraFrustumLength),
             rgba(kIconColor));
}

void draw_light_mark(const Transform& world) {
  DrawSphereWires(to_raylib(world.position), kIconSize, kIconRings, kIconSlices, rgba(kSunColor));
  DrawLine3D(to_raylib(world.position), to_raylib(world.position + world.forward() * kSunArrowLength), rgba(kSunColor));
}

void draw_mark(const GameObject& object, const Transform& world) {
  if (object.camera) {
    draw_camera_mark(world);
  } else if (object.light) {
    draw_light_mark(world);
  } else if (object.mesh == kNoIndex) {
    DrawCubeWires(to_raylib(world.position), kIconSize, kIconSize, kIconSize, rgba(kIconColor));
  }
}

void draw_line(Vec3 from, Vec3 to) { DrawLine3D(to_raylib(from), to_raylib(to), rgba(kJointColor)); }

void draw_polyline(const std::vector<Vec3>& points) {
  for (std::size_t i = 1; i < points.size(); ++i) draw_line(points[i - 1], points[i]);
}

// A joint: a small ball at its anchor, its axis, its limits, and a line to its other end.
void draw_joint_mark(const JointGizmo& joint) {
  DrawSphereWires(to_raylib(joint.anchor), kIconSize * kHalf, kIconRings, kIconSlices, rgba(kJointColor));
  if (joint.axis_start && joint.axis_end) draw_line(*joint.axis_start, *joint.axis_end);
  draw_polyline(joint.limits);
  if (joint.other_end) draw_line(joint.anchor, *joint.other_end);
}

// What a picture of the scene would not show: triggers, cameras, lights, empty objects and joints.
void draw_object_marks(const Scene& scene, const SceneDesc& desc) {
  for (const ObjectId id : scene.alive_ids()) {
    const GameObject& object = scene.at(id);
    const Transform world = scene.world_transform(id);
    if (body_config_from_props(object.props).trigger) draw_box(world, bounds_of(object, desc), rgba(kTriggerColor));
    draw_mark(object, world);
    if (const auto joint = joint_gizmo(scene, id)) draw_joint_mark(*joint);
  }
}

void draw_selection_box(const Scene& scene, const SceneDesc& desc, ObjectId selected) {
  if (!scene.is_alive(selected)) return;
  draw_box(scene.world_transform(selected), bounds_of(scene.at(selected), desc), rgba(kSelectionColor));
}

// The terrain brush: a ring lying on the ground, and its center.
void draw_brush(const BrushMark& brush) {
  DrawCircle3D(to_raylib(brush.center + Vec3::up() * kBrushLift), brush.radius, kWorldRight, kQuarterTurn,
               rgba(kSelectionColor));
  DrawLine3D(to_raylib(brush.center), to_raylib(brush.center + Vec3::up() * brush.radius * kBrushPoleShare),
             rgba(kSelectionColor));
}

}  // namespace

::Color rgba(const unsigned char (&c)[config::kChannelsPerColor]) { return {c[kRed], c[kGreen], c[kBlue], c[kAlpha]}; }

Bounds bounds_of(const GameObject& object, const SceneDesc& desc) {
  return object.mesh != kNoIndex ? desc.meshes[object.mesh].bounds : default_collider_bounds();
}

void draw_scene_overlays(const Scene& scene, const SceneDesc& desc, const std::vector<ObjectId>& selected,
                         const std::optional<BrushMark>& brush) {
  DrawGrid(kGridSlices, kGridSpacing);
  draw_object_marks(scene, desc);
  for (const ObjectId id : selected) draw_selection_box(scene, desc, id);
  if (brush) draw_brush(*brush);
}

}  // namespace redgem
