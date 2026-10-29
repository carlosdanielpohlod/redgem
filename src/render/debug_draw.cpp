#include "render/debug_draw.hpp"

#include <cmath>

namespace redgem {
namespace {
constexpr float kHalfSize = 0.5f;  // box center and size become two finite opposite corners
bool valid_channel(float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; }
bool valid_color(Color color) {
  return valid_channel(color.r) && valid_channel(color.g) && valid_channel(color.b) && valid_channel(color.a);
}
bool finite_vector(Vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
bool finite_fields(const DebugDrawItem& item) {
  return finite_vector(item.first) && finite_vector(item.second) && valid_color(item.color);
}
bool nonnegative(Vec3 size) { return size.x >= 0.0f && size.y >= 0.0f && size.z >= 0.0f; }
bool valid_box(const DebugDrawItem& item) {
  const Vec3 half = item.second * kHalfSize;
  return nonnegative(item.second) && finite_vector(item.first - half) && finite_vector(item.first + half);
}
bool valid(const DebugDrawItem& item) {
  if (!finite_fields(item)) return false;
  switch (item.kind) {
    case DebugDrawItem::Kind::Line:
      return true;
    case DebugDrawItem::Kind::Box:
      return valid_box(item);
  }
  return false;
}
}  // namespace
bool valid_debug_draw(const DebugDrawItem& item) { return valid(item); }
bool DebugDrawBuffer::append(const DebugDrawItem& item) {
  if (!valid(item)) return false;
  if (items_.size() == kMaxDebugDrawCommands) {
    ++dropped_;
    return false;
  }
  items_.push_back(item);
  return true;
}
void DebugDrawBuffer::clear() {
  items_.clear();
  dropped_ = 0;
}
}  // namespace redgem
