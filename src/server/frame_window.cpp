#include "server/frame_window.hpp"

#include <algorithm>

namespace redgem::server {

void FrameTimes::add(const FrameTimes& other) {
  frames += other.frames;
  total_seconds += other.total_seconds;
  worst_seconds = std::max(worst_seconds, other.worst_seconds);
}

double FrameTimes::average_seconds() const { return frames == 0 ? 0.0 : total_seconds / static_cast<double>(frames); }

void FrameWindow::note(double seconds) {
  seconds_[next_] = seconds;
  next_ = (next_ + 1) % kFrameWindowFrames;
  held_ = std::min(held_ + 1, kFrameWindowFrames);
}

FrameTimes FrameWindow::times() const {
  FrameTimes times;
  // (Until the window is full, the frames held are its first ones.)
  for (std::size_t i = 0; i < held_; ++i) times.add({1, seconds_[i], seconds_[i]});
  return times;
}

}  // namespace redgem::server
