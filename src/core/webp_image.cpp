#include "core/webp_image.hpp"

#include <webp/decode.h>

#include <algorithm>
#include <array>

namespace redgem {
namespace {

constexpr std::array<std::uint8_t, 4> kRiffMagic{'R', 'I', 'F', 'F'};
constexpr std::array<std::uint8_t, 4> kWebpMagic{'W', 'E', 'B', 'P'};
constexpr std::size_t kWebpMagicAt = 8;
constexpr std::size_t kWebpHeaderBytes = 12;

bool starts_as_webp(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr) return false;
  if (size < kWebpHeaderBytes) return false;
  return std::equal(kRiffMagic.begin(), kRiffMagic.end(), data) &&
         std::equal(kWebpMagic.begin(), kWebpMagic.end(), data + kWebpMagicAt);
}
bool static_webp(const WebPBitstreamFeatures& features) {
  return features.width > 0 && features.height > 0 && features.has_animation == 0;
}
bool valid_output(const std::uint8_t* output, std::size_t capacity, int stride) {
  return output != nullptr && capacity > 0 && stride > 0;
}

}  // namespace

std::optional<ImageSize> webp_image_size(const std::uint8_t* data, std::size_t size) {
  if (!starts_as_webp(data, size)) return std::nullopt;
  WebPBitstreamFeatures features{};
  if (WebPGetFeatures(data, size, &features) != VP8_STATUS_OK) return std::nullopt;
  if (!static_webp(features)) return std::nullopt;
  return ImageSize{static_cast<std::uint32_t>(features.width), static_cast<std::uint32_t>(features.height)};
}
bool decode_webp_rgba_into(const std::uint8_t* data, std::size_t size, std::uint8_t* output, std::size_t capacity,
                           int stride) {
  if (!valid_output(output, capacity, stride)) return false;
  if (!decodable_image(data, size)) return false;
  if (!webp_image_size(data, size)) return false;
  return WebPDecodeRGBAInto(data, size, output, capacity, stride) != nullptr;
}

}  // namespace redgem
