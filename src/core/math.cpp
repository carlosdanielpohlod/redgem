#include "core/math.hpp"

#include <algorithm>

namespace redgem {
namespace {

constexpr float kOne = 1.0f;
constexpr float kQuarter = 0.25f;
// |sin(pitch)| above which pitch is treated as exactly +/-90 degrees.
constexpr float kGimbalLockSinPitch = 0.9999f;

}  // namespace

Quat quat_from_euler_degrees(Vec3 degrees) {
  const Quat qx = quat_from_axis_angle(Vec3::right(), degrees_to_radians(degrees.x));
  const Quat qy = quat_from_axis_angle(Vec3::up(), degrees_to_radians(degrees.y));
  const Quat qz = quat_from_axis_angle(Vec3::back(), degrees_to_radians(degrees.z));
  return normalize(qz * qy * qx);
}

Vec3 quat_to_euler_degrees(Quat q) {
  q = normalize(q);
  // Inverse of R = Rz * Ry * Rx, read from the rotation matrix elements r(row)(column).
  const float r00 = kOne - kTwo * (q.y * q.y + q.z * q.z);
  const float r10 = kTwo * (q.x * q.y + q.w * q.z);
  const float r11 = kOne - kTwo * (q.x * q.x + q.z * q.z);
  const float r12 = kTwo * (q.y * q.z - q.w * q.x);
  const float r20 = kTwo * (q.x * q.z - q.w * q.y);
  const float r21 = kTwo * (q.y * q.z + q.w * q.x);
  const float r22 = kOne - kTwo * (q.x * q.x + q.y * q.y);

  const float sin_pitch = std::clamp(-r20, -kOne, kOne);
  float x;
  float y;
  float z;
  if (std::fabs(sin_pitch) < kGimbalLockSinPitch) {
    x = std::atan2(r21, r22);
    y = std::asin(sin_pitch);
    z = std::atan2(r10, r00);
  } else {
    // Gimbal lock: pitch is +/-90 degrees and X and Z turn around the same axis, so fold them into X.
    x = std::atan2(-r12, r11);
    y = std::copysign(kPi * kHalf, sin_pitch);
    z = 0.0f;
  }
  return {radians_to_degrees(x), radians_to_degrees(y), radians_to_degrees(z)};
}

Quat quat_from_basis(Vec3 c0, Vec3 c1, Vec3 c2) {
  // Shepperd's method on the matrix with columns c0, c1, c2.
  const float m00 = c0.x, m10 = c0.y, m20 = c0.z;
  const float m01 = c1.x, m11 = c1.y, m21 = c1.z;
  const float m02 = c2.x, m12 = c2.y, m22 = c2.z;
  const float trace = m00 + m11 + m22;
  Quat q;
  if (trace > 0.0f) {
    const float s = std::sqrt(trace + kOne) * kTwo;
    q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, kQuarter * s};
  } else if (m00 > m11 && m00 > m22) {
    const float s = std::sqrt(kOne + m00 - m11 - m22) * kTwo;
    q = {kQuarter * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s};
  } else if (m11 > m22) {
    const float s = std::sqrt(kOne + m11 - m00 - m22) * kTwo;
    q = {(m01 + m10) / s, kQuarter * s, (m12 + m21) / s, (m02 - m20) / s};
  } else {
    const float s = std::sqrt(kOne + m22 - m00 - m11) * kTwo;
    q = {(m02 + m20) / s, (m12 + m21) / s, kQuarter * s, (m10 - m01) / s};
  }
  return normalize(q);
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
