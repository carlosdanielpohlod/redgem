#pragma once

// Radiance pictures (.hdr, "RGBE"): light brighter than white, as sky pictures for lighting are
// usually saved. They come from outside, so nothing in them is trusted: the header is checked
// (its kind, its size, within the limits of image_header.hpp) before a pixel is decoded, and every
// run of pixels is checked against the scanline and the file it is in.
// Read: the usual layout (rows from the top, "-Y height +X width"), flat or run-length encoded
// scanlines. Not read: the XYZ kind and the rare older run-length encoding (such scanlines are
// taken as flat, which decodes wrong colors but never reads past the file).

#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/float_image.hpp"

namespace redgem {

// Where a Radiance picture's pixels start, and its size.
struct RadianceLayout {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::size_t pixels_at = 0;
};

// Whether the bytes start as a Radiance picture does ("#?" and the name of what wrote it).
bool is_radiance(const std::uint8_t* data, std::size_t size);

// The layout its header says; nullopt for one that is not a Radiance picture of the RGBE kind,
// is cut short, or is larger than kMaxImageSide along a side.
std::optional<RadianceLayout> radiance_layout(const std::uint8_t* data, std::size_t size);

// The picture in linear light, shrunk by a power of two until it is at most `widest` pixels wide
// (blocks of pixels averaged as they are read, so a large file never takes much memory). nullopt
// when the header is refused or a scanline is malformed or cut short.
std::optional<FloatImage> decode_radiance(const std::uint8_t* data, std::size_t size, std::uint32_t widest);

}  // namespace redgem
