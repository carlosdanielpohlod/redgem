#pragma once

// How big a picture is, read from the first bytes of its file without decoding it. A picture's
// header is a claim about how much memory decoding it will take: it is checked before any is.

#include <cstddef>
#include <cstdint>
#include <optional>

namespace redgem {

// The most bytes of an encoded picture that are decoded, and the most pixels along each of its
// sides (larger than any graphics card of the machines the engine runs on takes as one texture).
inline constexpr std::size_t kMaxEncodedImageBytes = 256u * 1024u * 1024u;
inline constexpr std::uint32_t kMaxImageSide = 8192;

struct ImageSize {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

// The size a PNG or JPEG file says its picture has; nullopt for anything that is neither, or
// whose header is cut short. (glTF pictures are one or the other.)
std::optional<ImageSize> encoded_image_size(const std::uint8_t* data, std::size_t size);

// Whether an encoded picture may be decoded: a PNG or JPEG within the limits above, with at least
// one pixel.
bool decodable_image(const std::uint8_t* data, std::size_t size);

}  // namespace redgem
