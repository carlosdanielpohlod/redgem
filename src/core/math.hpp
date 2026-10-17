#pragma once

// Tiny engine-owned math types so modules do not depend on raylib or Jolt types.

#include <cmath>
#include <cstddef>

namespace redgem {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kDegreesPerHalfTurn = 180.0f;
inline constexpr float kEpsilon = 1e-6f;
inline constexpr float kHalf = 0.5f;
inline constexpr float kTwo = 2.0f;

inline float degrees_to_radians(float degrees) { return degrees * kPi / kDegreesPerHalfTurn; }
inline float radians_to_degrees(float radians) { return radians * kDegreesPerHalfTurn / kPi; }

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  static Vec3 zero() { return {0.0f, 0.0f, 0.0f}; }
  static Vec3 one() { return {1.0f, 1.0f, 1.0f}; }
  static Vec3 up() { return {0.0f, 1.0f, 0.0f}; }
  static Vec3 right() { return {1.0f, 0.0f, 0.0f}; }
  // glTF / raylib convention: objects and cameras look down -Z.
  static Vec3 forward() { return {0.0f, 0.0f, -1.0f}; }
  static Vec3 back() { return {0.0f, 0.0f, 1.0f}; }
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(float s, Vec3 a) { return a * s; }
inline Vec3 operator/(Vec3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }
inline Vec3 hadamard(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline Vec3 safe_divide(Vec3 a, Vec3 b) {
  auto div = [](float n, float d) { return std::fabs(d) > kEpsilon ? n / d : 0.0f; };
  return {div(a.x, b.x), div(a.y, b.y), div(a.z, b.z)};
}
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(Vec3 v) {
  const float len = length(v);
  return len > kEpsilon ? v / len : Vec3::zero();
}
// The point `t` of the way from `a` to `b`: `a` at 0, `b` at 1.
inline Vec3 mix(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
inline bool approx_equal(float a, float b, float tolerance) { return std::fabs(a - b) <= tolerance; }
inline bool approx_equal(Vec3 a, Vec3 b, float tolerance) {
  return approx_equal(a.x, b.x, tolerance) && approx_equal(a.y, b.y, tolerance) && approx_equal(a.z, b.z, tolerance);
}

struct Quat {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 1.0f;

  static Quat identity() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
};

// Where each component sits when a vector (x y z) or a quaternion (x y z w) is stored as a run
// of floats, as meshes, matrices and animation keys store them.
inline constexpr std::size_t kXOffset = 0;
inline constexpr std::size_t kYOffset = 1;
inline constexpr std::size_t kZOffset = 2;
inline constexpr std::size_t kWOffset = 3;

inline Vec3 vec3_from(const float* floats) { return {floats[kXOffset], floats[kYOffset], floats[kZOffset]}; }
inline Quat quat_from(const float* floats) {
  return {floats[kXOffset], floats[kYOffset], floats[kZOffset], floats[kWOffset]};
}

inline Quat operator*(Quat a, Quat b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline Quat conjugate(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }
inline Quat normalize(Quat q) {
  const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (len <= kEpsilon) return Quat::identity();
  return {q.x / len, q.y / len, q.z / len, q.w / len};
}
inline Vec3 rotate(Quat q, Vec3 v) {
  const Vec3 u{q.x, q.y, q.z};
  const Vec3 t = cross(u, v) * kTwo;
  return v + t * q.w + cross(u, t);
}
inline Quat quat_from_axis_angle(Vec3 axis, float radians) {
  const Vec3 n = normalize(axis);
  const float s = std::sin(radians * kHalf);
  return {n.x * s, n.y * s, n.z * s, std::cos(radians * kHalf)};
}
// Euler angles in degrees, applied in X, then Y, then Z order (intrinsic Z * Y * X).
Quat quat_from_euler_degrees(Vec3 degrees);
Vec3 quat_to_euler_degrees(Quat q);
// Rotation that makes local -Z point along `direction`, keeping `up` as close as possible.
Quat quat_look_rotation(Vec3 direction, Vec3 up);
// Rotation matrix given as three orthonormal basis columns.
Quat quat_from_basis(Vec3 x_axis, Vec3 y_axis, Vec3 z_axis);
bool approx_equal(Quat a, Quat b, float tolerance);

}  // namespace redgem
