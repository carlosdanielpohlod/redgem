#pragma once

#include <cstddef>
#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem {
inline constexpr std::size_t kMaxDebugDrawCommands = 4096;  // per frame, independent of scene size
struct DebugDrawItem {
  enum class Kind { Line, Box };
  Kind kind = Kind::Line;
  Vec3 first;   // line start or box center, in world coordinates
  Vec3 second;  // line end or nonnegative box size
  Color color;
  bool depth_test = true;
};

bool valid_debug_draw(const DebugDrawItem& item);

// One owning thread, one frame. Items never become scene objects or GPU resources. A full buffer
// counts dropped commands without growing; clearing a frame retains capacity for the next one.
class DebugDrawBuffer {
 public:
  bool append(const DebugDrawItem& item);
  void clear();
  const std::vector<DebugDrawItem>& items() const { return items_; }
  std::size_t dropped() const { return dropped_; }

 private:
  std::vector<DebugDrawItem> items_;
  std::size_t dropped_ = 0;
};
}  // namespace redgem
