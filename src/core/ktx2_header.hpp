#pragma once

// What a KTX2 file of Basis Universal data (KHR_texture_basisu) says it holds, read from its
// header and level index without decoding anything. Only what the engine draws passes: one 2D
// picture (no array, cube map or volume), of Basis data (no other Vulkan format), within the
// picture limits of image_header.hpp, every section and mipmap level it lists inside the file.

#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/image_header.hpp"

namespace redgem {

// How a KTX2 file's levels are stored (its supercompressionScheme).
enum class Ktx2Supercompression : std::uint32_t { None = 0, BasisLz = 1, Zstandard = 2 };

struct Ktx2Header {
  ImageSize size;
  std::uint32_t levels = 1;  // mipmap levels stored, the full-size picture first
  Ktx2Supercompression supercompression = Ktx2Supercompression::None;
};

// Whether the bytes start with KTX2's identifier.
bool starts_as_ktx2(const std::uint8_t* data, std::size_t size);

// The header of a KTX2 file the engine can draw; nullopt for anything else, or a header that is
// cut short or lies about where its levels are.
std::optional<Ktx2Header> read_ktx2_header(const std::uint8_t* data, std::size_t size);

}  // namespace redgem
