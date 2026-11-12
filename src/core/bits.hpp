#pragma once

#include <cstdint>

namespace redgem {

// How many bits a byte holds: what a number is shifted by to move it one byte along.
inline constexpr int kBitsPerByte = 8;
// The lowest byte of a number, what is left of it once masked with this.
inline constexpr std::uint32_t kByteMask = 0xFF;

}  // namespace redgem
