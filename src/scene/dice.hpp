#pragma once

// Random numbers that are the same for the same seed on every machine and compiler (the
// standard library's distributions are not): SplitMix64. What is scattered over a level depends
// on them, and every machine of a game must scatter the same.

#include <cstdint>

namespace redgem {

class Dice {
 public:
  explicit Dice(std::uint64_t seed) : state_(seed) {}

  // From 0 up to (not including) 1.
  float next() {
    state_ += kStep;
    std::uint64_t mixed = state_;
    mixed = (mixed ^ (mixed >> kFirstShift)) * kFirstMix;
    mixed = (mixed ^ (mixed >> kSecondShift)) * kSecondMix;
    mixed ^= mixed >> kLastShift;
    return static_cast<float>(mixed >> kDroppedBits) * kPerUnit;
  }
  float between(float low, float high) { return low + (high - low) * next(); }

 private:
  static constexpr std::uint64_t kStep = 0x9E3779B97F4A7C15ULL;
  static constexpr std::uint64_t kFirstMix = 0xBF58476D1CE4E5B9ULL;
  static constexpr std::uint64_t kSecondMix = 0x94D049BB133111EBULL;
  static constexpr int kFirstShift = 30;
  static constexpr int kSecondShift = 27;
  static constexpr int kLastShift = 31;
  static constexpr int kDroppedBits = 40;                // the 24 bits a float holds exactly are kept
  static constexpr float kPerUnit = 1.0f / 16777216.0f;  // 2^-24
  std::uint64_t state_;
};

}  // namespace redgem
