#include "core/math.hpp"

#include <algorithm>

namespace redgem {
namespace {

constexpr float kOne = 1.0f;
constexpr float kQuarter = 0.25f;
// |sin(pitch)| above which pitch is treated as exactly +/-90 degrees.
constexpr float kGimbalLockSinPitch = 0.9999f;

// The elements r(row)(column) of a rotation matrix that its Euler angles are read from.
struct EulerElements {
  float r00, r10, r11, r12, r20, r21, r22;
};

EulerElements euler_elements(Quat q) {
  return {kOne - kTwo * (q.y * q.y + q.z * q.z), kTwo * (q.x * q.y + q.w * q.z), kOne - kTwo * (q.x * q.x + q.z * q.z),
          kTwo * (q.y * q.z - q.w * q.x),        kTwo * (q.x * q.z - q.w * q.y), kTwo * (q.y * q.z + q.w * q.x),
          kOne - kTwo * (q.x * q.x + q.y * q.y)};
}

Vec3 free_euler_radians(const EulerElements& r, float sin_pitch) {
  return {std::atan2(r.r21, r.r22), std::asin(sin_pitch), std::atan2(r.r10, r.r00)};
}

// Gimbal lock: pitch is +/-90 degrees and X and Z turn around the same axis, so fold them into X.
Vec3 locked_euler_radians(const EulerElements& r, float sin_pitch) {
  return {std::atan2(-r.r12, r.r11), std::copysign(kPi * kHalf, sin_pitch), 0.0f};
}

// A rotation matrix, elements named m(row)(column).
struct Matrix3 {
  float m00, m10, m20;
  float m01, m11, m21;
  float m02, m12, m22;
};

// Shepperd's method has one formula per largest term (the trace, or one of the diagonal's
// elements), so that the division is never by a small number.
Quat quat_from_large_trace(const Matrix3& m) {
  const float s = std::sqrt(m.m00 + m.m11 + m.m22 + kOne) * kTwo;
  return {(m.m21 - m.m12) / s, (m.m02 - m.m20) / s, (m.m10 - m.m01) / s, kQuarter * s};
}

Quat quat_from_large_m00(const Matrix3& m) {
  const float s = std::sqrt(kOne + m.m00 - m.m11 - m.m22) * kTwo;
  return {kQuarter * s, (m.m01 + m.m10) / s, (m.m02 + m.m20) / s, (m.m21 - m.m12) / s};
}

Quat quat_from_large_m11(const Matrix3& m) {
  const float s = std::sqrt(kOne + m.m11 - m.m00 - m.m22) * kTwo;
  return {(m.m01 + m.m10) / s, kQuarter * s, (m.m12 + m.m21) / s, (m.m02 - m.m20) / s};
}

Quat quat_from_large_m22(const Matrix3& m) {
  const float s = std::sqrt(kOne + m.m22 - m.m00 - m.m11) * kTwo;
  return {(m.m02 + m.m20) / s, (m.m12 + m.m21) / s, kQuarter * s, (m.m10 - m.m01) / s};
}

bool largest_is_m00(const Matrix3& m) { return m.m00 > m.m11 && m.m00 > m.m22; }

Quat quat_from_matrix(const Matrix3& m) {
  if (m.m00 + m.m11 + m.m22 > 0.0f) return quat_from_large_trace(m);
  if (largest_is_m00(m)) return quat_from_large_m00(m);
  if (m.m11 > m.m22) return quat_from_large_m11(m);
  return quat_from_large_m22(m);
}

}  // namespace

Quat quat_from_euler_degrees(Vec3 degrees) {
  const Quat qx = quat_from_axis_angle(Vec3::right(), degrees_to_radians(degrees.x));
  const Quat qy = quat_from_axis_angle(Vec3::up(), degrees_to_radians(degrees.y));
  const Quat qz = quat_from_axis_angle(Vec3::back(), degrees_to_radians(degrees.z));
  return normalize(qz * qy * qx);
}

Vec3 quat_to_euler_degrees(Quat q) {
  // Inverse of R = Rz * Ry * Rx.
  const EulerElements r = euler_elements(normalize(q));
  const float sin_pitch = std::clamp(-r.r20, -kOne, kOne);
  const bool locked = std::fabs(sin_pitch) >= kGimbalLockSinPitch;
  const Vec3 radians = locked ? locked_euler_radians(r, sin_pitch) : free_euler_radians(r, sin_pitch);
  return {radians_to_degrees(radians.x), radians_to_degrees(radians.y), radians_to_degrees(radians.z)};
}

Quat quat_from_basis(Vec3 c0, Vec3 c1, Vec3 c2) {
  return normalize(quat_from_matrix({c0.x, c0.y, c0.z, c1.x, c1.y, c1.z, c2.x, c2.y, c2.z}));
}

Quat quat_look_rotation(Vec3 direction, Vec3 up) {
  const Vec3 back = normalize(-direction);  // local +Z points away from the target
  if (length(back) <= kEpsilon) return Quat::identity();
  Vec3 right = cross(up, back);
  if (length(right) <= kEpsilon) right = cross(Vec3::forward(), back);  // looking straight up/down
  right = normalize(right);
  const Vec3 true_up = cross(back, right);
  return quat_from_basis(right, true_up, back);
}

bool approx_equal(Quat a, Quat b, float tolerance) {
  // q and -q represent the same rotation.
  const float d = std::fabs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
  return std::fabs(d - kOne) <= tolerance;
}

}  // namespace redgem
