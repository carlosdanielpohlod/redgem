#pragma once

#include <cstddef>
#include <string>
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

inline constexpr std::size_t kMaxDebugLabels = 256;      // per frame
inline constexpr std::size_t kMaxDebugLabelBytes = 256;  // of one label's text
inline constexpr int kMinDebugLabelSize = 1;             // text height, in pixels
inline constexpr int kMaxDebugLabelSize = 256;
// A text shown where a point of the world is on the screen (Debug.label): centered on it, in the
// interface's pixels, for this frame only.
struct DebugLabel {
  std::string text;
  Vec3 at;  // in world coordinates
  Color color;
  int size = kMinDebugLabelSize;
};

bool valid_debug_label(const DebugLabel& label);

// One owning thread, one frame. Items never become scene objects or GPU resources. A full buffer
// counts dropped commands without growing; clearing a frame retains capacity for the next one.
class DebugDrawBuffer {
 public:
  bool append(const DebugDrawItem& item);
  bool append(const DebugLabel& label);
  void clear();
  const std::vector<DebugDrawItem>& items() const { return items_; }
  const std::vector<DebugLabel>& labels() const { return labels_; }
  std::size_t dropped() const { return dropped_; }
  std::size_t dropped_labels() const { return dropped_labels_; }

 private:
  std::vector<DebugDrawItem> items_;
  std::vector<DebugLabel> labels_;
  std::size_t dropped_ = 0;
  std::size_t dropped_labels_ = 0;
};
}  // namespace redgem
