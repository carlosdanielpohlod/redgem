#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/image_header.hpp"

namespace redgem {

// Only static RIFF/WebP images; no allocation while reading the dimensions.
std::optional<ImageSize> webp_image_size(const std::uint8_t* data, std::size_t size);
// Uses previously checked metadata, without parsing the header again.
bool decode_webp_rgba_into(const EncodedImage& image, std::uint8_t* output, std::size_t capacity, int stride);
// Decodes to the caller's RGBA8 buffer. Invalid/animated images or an insufficient output return false.
bool decode_webp_rgba_into(const std::uint8_t* data, std::size_t size, std::uint8_t* output, std::size_t capacity,
                           int stride);

}  // namespace redgem
