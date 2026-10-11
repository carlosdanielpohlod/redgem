#pragma once

#include "core/math.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// Per-frame controls of the editor fly camera (already scaled to the frame).
struct FlyInput {
  float look_x = 0.0f;  // mouse delta in pixels while looking (right mouse button held)
  float look_y = 0.0f;
  float forward = 0.0f;  // -1..1 (W/S)
  float right = 0.0f;    // -1..1 (D/A)
  float up = 0.0f;       // -1..1 (E/Q)
  float zoom = 0.0f;     // mouse wheel notches
  bool fast = false;     // shift held
  float dt = 0.0f;
};

// Fly-through scene camera: hold right mouse to look around and fly with WASD/QE, wheel to dolly.
class EditorCamera {
 public:
  EditorCamera();

  void update(const FlyInput& input);
  // Moves the camera so `bounds` (in world space, center + radius) fills the view.
  void frame(Vec3 center, float radius);
  void look_at(Vec3 position, Vec3 target);

  Vec3 position() const { return position_; }
  Vec3 forward() const;
  Vec3 right() const;
  Vec3 target() const { return position_ + forward(); }
  float fov_y_degrees() const;

 private:
  Vec3 position_;
  float yaw_degrees_ = 0.0f;    // 0 looks down -Z
  float pitch_degrees_ = 0.0f;  // positive looks up
};

}  // namespace redgem
