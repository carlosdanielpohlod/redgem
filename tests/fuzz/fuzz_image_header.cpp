// What is read of a picture before it is decoded (the size its header claims, PNG, JPEG, WebP and
// Radiance), and the decoding of the engine's own Radiance reader and of small WebP pictures.
#include <cstdint>
#include <vector>

#include "core/image_header.hpp"
#include "core/radiance_image.hpp"
#include "core/webp_image.hpp"
#include "fuzz/fuzz_target.hpp"

namespace {

// The widest a Radiance picture is decoded to (larger ones are shrunk as they are read).
constexpr std::uint32_t kWidestRadiance = 64;
// The most pixels a WebP picture may have to be decoded here: enough for any made-up input that
// gets through its header, small enough that each run stays quick.
constexpr std::uint64_t kMostWebpPixels = 256 * 256;
constexpr std::uint32_t kBytesPerPixel = 4;  // RGBA8

bool small_webp(const redgem::EncodedImage& image) {
  const redgem::ImageSize size = image.dimensions();
  return image.format() == redgem::EncodedImageFormat::Webp &&
         static_cast<std::uint64_t>(size.width) * size.height <= kMostWebpPixels;
}

void decode_small_webp(const std::uint8_t* data, std::size_t size) {
  const auto image = redgem::EncodedImage::read(data, size);
  if (!image || !small_webp(*image)) return;
  const std::uint32_t stride = image->dimensions().width * kBytesPerPixel;
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride) * image->dimensions().height);
  redgem::decode_webp_rgba_into(*image, pixels.data(), pixels.size(), static_cast<int>(stride));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  redgem::encoded_image_size(data, size);
  redgem::decodable_image(data, size);
  redgem::webp_image_size(data, size);
  decode_small_webp(data, size);
  if (redgem::is_radiance(data, size)) {
    redgem::radiance_layout(data, size);
    redgem::decode_radiance(data, size, kWidestRadiance);
  }
  return redgem::fuzz::kKeepInput;
}
