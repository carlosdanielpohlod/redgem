#pragma once

// The bytes a pose (pose_codec.hpp) and the animation it carries (animation_codec.hpp) are
// written in and read from: little-endian whole numbers, 32-bit floats as their bits, texts as
// they are.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "core/math.hpp"
#include "net/pose_quantization.hpp"

namespace redgem::net {

inline constexpr std::size_t kByte = 1;
inline constexpr std::size_t kFloatBytes = 4;
inline constexpr std::size_t kStepBytes = 2;
inline constexpr int kBitsPerByte = 8;
inline constexpr std::uint32_t kByteMask = 0xFF;

class PoseWriter {
 public:
  void byte(std::uint8_t value) { bytes_.push_back(value); }
  // The `size` lowest bytes of `value`, lowest first.
  void word(std::uint32_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) byte(static_cast<std::uint8_t>((value >> (i * kBitsPerByte)) & kByteMask));
  }
  void text(const std::string& value) { bytes_.insert(bytes_.end(), value.begin(), value.end()); }
  void number(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    word(bits, kFloatBytes);
  }
  void vector(Vec3 value) {
    for (const float axis : {value.x, value.y, value.z}) number(axis);
  }
  void steps(const PoseSteps& value) {
    for (const std::int16_t axis : value) word(static_cast<std::uint16_t>(axis), kStepBytes);
  }
  std::vector<std::uint8_t> take() { return std::move(bytes_); }

 private:
  std::vector<std::uint8_t> bytes_;
};

// Reads bytes known to be there: whoever reads checks first, with left(), that they are.
class PoseReader {
 public:
  PoseReader(const std::vector<std::uint8_t>& bytes, std::size_t at) : bytes_(bytes), at_(at) {}

  std::size_t left() const { return bytes_.size() - at_; }
  std::uint32_t word(std::size_t size) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < size; ++i) value |= static_cast<std::uint32_t>(bytes_[at_ + i]) << (i * kBitsPerByte);
    at_ += size;
    return value;
  }
  std::string text(std::size_t size) {
    const auto first = bytes_.begin() + static_cast<std::ptrdiff_t>(at_);
    at_ += size;
    return {first, first + static_cast<std::ptrdiff_t>(size)};
  }
  float number() {
    const std::uint32_t bits = word(kFloatBytes);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }
  Vec3 vector() {
    const float x = number();
    const float y = number();
    return {x, y, number()};
  }
  std::int16_t step() { return static_cast<std::int16_t>(static_cast<std::uint16_t>(word(kStepBytes))); }
  PoseSteps steps() {
    const std::int16_t x = step();
    const std::int16_t y = step();
    return {x, y, step()};
  }

 private:
  const std::vector<std::uint8_t>& bytes_;
  std::size_t at_;
};

}  // namespace redgem::net
