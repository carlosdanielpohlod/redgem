#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Numbers in the bytes of the files icons are kept in: Windows' are little-endian (the lowest byte
// first), Apple's big-endian.
namespace redgem::bytes {

using Bytes = std::vector<std::uint8_t>;

inline constexpr unsigned kBitsPerByte = 8;
inline constexpr std::uint32_t kByteMask = 0xFF;

// Adds `count` bytes of `value` at the end of `out`.
inline void append_little(Bytes& out, std::uint32_t value, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i)
    out.push_back(static_cast<std::uint8_t>((value >> (kBitsPerByte * i)) & kByteMask));
}
inline void append_big(Bytes& out, std::uint32_t value, std::size_t count) {
  for (std::size_t i = count; i > 0; --i) {
    out.push_back(static_cast<std::uint8_t>((value >> (kBitsPerByte * (i - 1))) & kByteMask));
  }
}

// Reads and writes `count` bytes at `at`; whoever calls has checked that they are inside.
inline std::uint32_t read_little(const Bytes& in, std::size_t at, std::size_t count) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < count; ++i) value |= static_cast<std::uint32_t>(in[at + i]) << (kBitsPerByte * i);
  return value;
}
inline std::uint32_t read_big(const Bytes& in, std::size_t at, std::size_t count) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < count; ++i) value = (value << kBitsPerByte) | in[at + i];
  return value;
}
inline void write_little(Bytes& out, std::size_t at, std::uint32_t value, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i)
    out[at + i] = static_cast<std::uint8_t>((value >> (kBitsPerByte * i)) & kByteMask);
}

inline void write_big(Bytes& out, std::size_t at, std::uint32_t value, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    out[at + i] = static_cast<std::uint8_t>((value >> (kBitsPerByte * (count - 1 - i))) & kByteMask);
  }
}

// Whether `count` bytes from `at` are inside a file of `size` bytes (with no overflow on the way).
inline bool inside(std::size_t size, std::size_t at, std::size_t count) { return at <= size && count <= size - at; }

}  // namespace redgem::bytes
