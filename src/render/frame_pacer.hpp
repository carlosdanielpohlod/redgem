#pragma once

// Keeps drawing that never shows a frame on a screen (a shot) from running
// ahead of the graphics card. A window's frames are held back by the swap that shows them; frames
// drawn off screen are not, and the commands of hundreds of frames then pile up in the driver
// until it drops work: on a heavy scene (tens of thousands of scattered copies) a picture read
// back a few seconds in came out black. The pacer lets at most a few frames be in flight: after
// each one it marks the card's queue, and waits for the mark of the oldest once there are too
// many.

#include <cstddef>
#include <deque>
#include <functional>

#include "render/gl_extras.hpp"

namespace redgem {

// Frames the card may be behind: two lets it work on one while the next is sent.
inline constexpr std::size_t kFramesAhead = 2;

// How the pacer marks and waits (the card's sync objects; tests count instead).
struct FenceCalls {
  std::function<render_detail::CardFence()> insert;
  std::function<void(render_detail::CardFence)> wait;
  std::function<void(render_detail::CardFence)> release;
};
// The graphics card's: needs a window's graphics context.
FenceCalls card_fences();

class FramePacer {
 public:
  explicit FramePacer(std::size_t frames_ahead = kFramesAhead, FenceCalls calls = card_fences());
  ~FramePacer();
  FramePacer(const FramePacer&) = delete;
  FramePacer& operator=(const FramePacer&) = delete;

  // After a frame's commands are sent: marks them, and waits while more than frames_ahead are in
  // flight.
  void frame_drawn();
  std::size_t in_flight() const { return fences_.size(); }

 private:
  void wait_oldest();

  std::size_t frames_ahead_;
  FenceCalls calls_;
  std::deque<render_detail::CardFence> fences_;
};

}  // namespace redgem
