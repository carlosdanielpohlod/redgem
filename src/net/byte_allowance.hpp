#pragma once

#include <algorithm>
#include <cstddef>

namespace redgem::net {

// What one machine may send to a program that serves many (a relay, a dedicated server): so many
// bytes a second over time, and at once as much as a burst. A machine past it is flooding.
//
// Time the receiver stood still is forgiven apart (forgive): what the sender said meanwhile at its
// own pace may arrive late and all at once. That credit is spent first, and only on what it was
// given for: time keeps earning the ordinary allowance meanwhile, however long the backlog takes to
// arrive, so a sender that goes on talking at its pace is never cut off for it.
class ByteAllowance {
 public:
  ByteAllowance(float bytes_per_second, float burst_bytes)
      : per_second_(bytes_per_second), burst_(burst_bytes), left_(burst_bytes) {}

  // Time passing earns bytes back, up to the burst.
  void refill(float seconds) { left_ = std::min(burst_, left_ + seconds * per_second_); }
  // What `seconds` the receiver stood still earn, on top of the burst, until spent: added to what
  // earlier stalls left unspent, but never more in all than `most_seconds` earn.
  void forgive(float seconds, float most_seconds) {
    forgiven_ = std::min(forgiven_ + seconds * per_second_, most_seconds * per_second_);
  }
  // Takes what arrived out of it (what goes past the allowance out of what was forgiven); false
  // when that was more than was left.
  bool charge(std::size_t bytes) {
    left_ -= static_cast<float>(bytes);
    if (left_ < 0.0f) {
      const float covered = std::min(forgiven_, -left_);
      forgiven_ -= covered;
      left_ += covered;
    }
    return left_ >= 0.0f;
  }

 private:
  float per_second_;
  float burst_;
  float left_;
  float forgiven_ = 0.0f;  // what stalls earned past the burst, not spent yet
};

}  // namespace redgem::net
