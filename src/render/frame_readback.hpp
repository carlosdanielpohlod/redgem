#pragma once

// Reading finished frames back from the graphics card without waiting for it: each frame is read
// into one of a few pixel buffers in turn, and handed over a few frames later, when the card is
// long done with it. The same on desktop OpenGL 3.3 and OpenGL ES 3.

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace redgem {

class FrameReadback {
 public:
  // Called with each frame's pixels: three bytes a pixel, rows bottom first, `width` x `height`.
  using Consumer = std::function<void(const std::uint8_t* pixels, std::size_t bytes)>;

  FrameReadback(int width, int height);
  ~FrameReadback();
  FrameReadback(const FrameReadback&) = delete;
  FrameReadback& operator=(const FrameReadback&) = delete;

  // Starts reading the color of `framebuffer` (of this size), and hands the oldest frame still
  // being read to `consumer` once the ring of buffers is full.
  void read(unsigned int framebuffer, const Consumer& consumer);
  // Hands every frame still being read to `consumer`, the oldest first.
  void drain(const Consumer& consumer);

 private:
  static constexpr std::size_t kBuffers = 3;  // frames in flight

  void hand_over(std::size_t buffer, const Consumer& consumer);

  int width_;
  int height_;
  std::size_t bytes_;
  std::array<unsigned int, kBuffers> buffers_{};
  std::size_t next_ = 0;     // the buffer the next frame is read into
  std::size_t pending_ = 0;  // frames read and not handed over yet
};

}  // namespace redgem
