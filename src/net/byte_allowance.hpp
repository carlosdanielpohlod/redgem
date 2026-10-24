#pragma once

#include <algorithm>
#include <cstddef>

namespace redgem::net {

// What one machine may send to a program that serves many (a relay, a dedicated server): so many
// bytes a second over time, and at once as much as a burst. A machine past it is flooding.
class ByteAllowance {
 public:
  ByteAllowance(float bytes_per_second, float burst_bytes)
      : per_second_(bytes_per_second), burst_(burst_bytes), left_(burst_bytes) {}

  // Time passing earns bytes back, up to the burst (what was forgiven past it stays until used).
  void refill(float seconds) { left_ = std::max(left_, std::min(burst_, left_ + seconds * per_second_)); }
  // What `seconds` earn, past the burst too: for time the receiver stood still while the sender
  // went on at its pace, whose bytes then arrive all at once.
  void forgive(float seconds) { left_ = std::max(left_, 0.0f) + seconds * per_second_; }
  // Takes what arrived out of it; false when that was more than was left.
  bool charge(std::size_t bytes) {
    left_ -= static_cast<float>(bytes);
    return left_ >= 0.0f;
  }

 public:
  float left() const { return left_; }

 private:
  float per_second_;
  float burst_;
  float left_;
};

}  // namespace redgem::net
