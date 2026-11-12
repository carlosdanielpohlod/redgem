#pragma once

#include <array>
#include <cstddef>

// How long a dedicated server's rooms take for their frames (the turns of the threads they run on),
// over the last few seconds: what its status says (see server_status.hpp).
namespace redgem::server {

// Frames the window holds: five seconds of the engine's fixed step.
inline constexpr std::size_t kFrameWindowFrames = 300;

// A run of frames' times, in seconds: how many, all of them together, and the longest.
struct FrameTimes {
  std::size_t frames = 0;
  double total_seconds = 0.0;
  double worst_seconds = 0.0;

  // Adds another run (another room's) to this one.
  void add(const FrameTimes& other);
  // 0 when there was no frame.
  double average_seconds() const;
};

// The times of a room's last kFrameWindowFrames frames: each new one pushes the oldest out.
class FrameWindow {
 public:
  void note(double seconds);
  FrameTimes times() const;

 private:
  std::array<double, kFrameWindowFrames> seconds_{};
  std::size_t next_ = 0;  // where the next frame goes
  std::size_t held_ = 0;  // frames noted, up to the window's size
};

}  // namespace redgem::server
