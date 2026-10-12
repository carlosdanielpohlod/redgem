#include "editor/editor_camera.hpp"

#include <algorithm>
#include <cmath>

#include "editor/editor_logic.hpp"

namespace redgem {
namespace {

constexpr float kLookDegreesPerPixel = 0.25f;
constexpr float kMaxPitchDegrees = 89.0f;
constexpr float kFlySpeed = 8.0f;  // m/s
constexpr float kFastMultiplier = 4.0f;
constexpr float kZoomStep = 1.5f;  // meters per wheel notch
constexpr float kFovYDegrees = 50.0f;
constexpr float kFrameDistanceFactor = 2.5f;  // camera distance per unit of radius when framing
constexpr float kMinFrameRadius = 0.5f;
constexpr Vec3 kStartPosition{0.0f, 8.0f, 14.0f};
constexpr Vec3 kStartTarget{0.0f, 0.0f, 0.0f};

}  // namespace

void steer(FlyInput& input, const FlyKeys& keys) {
  input.forward = editor_logic::fly_axis(keys.back, keys.forward);
  input.right = editor_logic::fly_axis(keys.left, keys.right);
  input.up = editor_logic::fly_axis(keys.down, keys.up);
  input.fast = keys.left_shift || keys.right_shift;
}

EditorCamera::EditorCamera() { look_at(kStartPosition, kStartTarget); }

Vec3 EditorCamera::forward() const {
  const float yaw = degrees_to_radians(yaw_degrees_);
  const float pitch = degrees_to_radians(pitch_degrees_);
  return {-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
}

Vec3 EditorCamera::right() const { return normalize(cross(forward(), Vec3::up())); }

float EditorCamera::fov_y_degrees() const { return kFovYDegrees; }

void EditorCamera::update(const FlyInput& input) {
  yaw_degrees_ -= input.look_x * kLookDegreesPerPixel;
  pitch_degrees_ =
      std::clamp(pitch_degrees_ - input.look_y * kLookDegreesPerPixel, -kMaxPitchDegrees, kMaxPitchDegrees);
  const float speed = kFlySpeed * (input.fast ? kFastMultiplier : 1.0f);
  const Vec3 move = forward() * input.forward + right() * input.right + Vec3::up() * input.up;
  position_ = position_ + move * (speed * input.dt) + forward() * (input.zoom * kZoomStep);
}

void EditorCamera::look_at(Vec3 position, Vec3 target) {
  position_ = position;
  const Vec3 direction = normalize(target - position);
  if (length(direction) <= kEpsilon) return;
  pitch_degrees_ = std::clamp(radians_to_degrees(std::asin(direction.y)), -kMaxPitchDegrees, kMaxPitchDegrees);
  yaw_degrees_ = radians_to_degrees(std::atan2(-direction.x, -direction.z));
}

void EditorCamera::frame(Vec3 center, float radius) {
  const float distance = std::max(radius, kMinFrameRadius) * kFrameDistanceFactor;
  look_at(center - forward() * distance, center);
}

}  // namespace redgem
