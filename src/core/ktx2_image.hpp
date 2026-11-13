#pragma once

// KTX2 pictures of Basis Universal data (glTF's KHR_texture_basisu: ETC1S or UASTC LDR 4x4),
// transcoded on the CPU by the official Basis Universal transcoder into what the graphics card
// takes (see gpu_texture_formats.hpp). What the file claims is checked first (ktx2_header.hpp).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/gpu_texture_formats.hpp"
#include "core/image_header.hpp"

namespace redgem {

// One mipmap level: its size in pixels and its data, blocks (or RGBA8 pixels) row by row.
struct TranscodedLevel {
  ImageSize size;
  std::vector<std::uint8_t> bytes;
};

// Every level of a picture, the full-size one first, all in one format.
struct TranscodedPicture {
  GpuBlockFormat format = GpuBlockFormat::Rgba8;
  bool has_alpha = false;
  std::vector<TranscodedLevel> levels;
};

// Every level of a KTX2 picture in the best format a card that takes `support` has for it;
// nullopt when the file is not a KTX2 picture the engine reads, or its data is corrupt.
std::optional<TranscodedPicture> transcode_ktx2(const std::uint8_t* data, std::size_t size,
                                                const GpuTextureSupport& support);

// The full-size picture alone, as RGBA8 (four bytes a pixel, rows from the top).
std::optional<TranscodedLevel> decode_ktx2_rgba(const std::uint8_t* data, std::size_t size);

}  // namespace redgem
