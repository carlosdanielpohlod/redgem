#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Where each field of a game archive's header and table records is, shared by its reader
// (game_archive.cpp) and its writer (game_archive_writer.cpp). See game_archive.hpp for the format.
namespace redgem::archive_layout {

inline constexpr std::size_t kU16Bytes = 2;
inline constexpr std::size_t kU32Bytes = 4;
inline constexpr std::size_t kU64Bytes = 8;
inline constexpr unsigned kByteBits = 8;
inline constexpr std::uint8_t kReserved = 0;  // what a record's reserved byte holds

// The header's fields, after its magic.
inline constexpr std::size_t kVersionAt = 8;
inline constexpr std::size_t kCountAt = 12;
inline constexpr std::size_t kTableOffsetAt = 16;
inline constexpr std::size_t kTableSizeAt = 24;

// A table record's fields, before its name.
inline constexpr std::size_t kNameLengthAt = 0;
inline constexpr std::size_t kMethodAt = 2;
inline constexpr std::size_t kReservedAt = 3;
inline constexpr std::size_t kCrcAt = 4;
inline constexpr std::size_t kOffsetAt = 8;
inline constexpr std::size_t kSizeAt = 16;
inline constexpr std::size_t kStoredSizeAt = 24;

// A number of `count` bytes, little-endian, at `bytes`.
inline std::uint64_t little_endian(const std::uint8_t* bytes, std::size_t count) {
  std::uint64_t value = 0;
  for (std::size_t i = count; i > 0; --i) value = (value << kByteBits) | bytes[i - 1];
  return value;
}

// Appends a number as `count` bytes, little-endian.
inline void put_little_endian(std::vector<std::uint8_t>& out, std::uint64_t value, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) out.push_back(static_cast<std::uint8_t>(value >> (kByteBits * i)));
}

// Whether `count` bytes from `offset` lie inside `total` bytes (without the sum wrapping around).
inline bool fits(std::uint64_t offset, std::uint64_t count, std::uint64_t total) {
  return offset <= total && count <= total - offset;
}

}  // namespace redgem::archive_layout
