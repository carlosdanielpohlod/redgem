#include "render/sky_picture.hpp"

#include <raylib.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "core/image_header.hpp"
#include "core/log.hpp"
#include "core/radiance_image.hpp"
#include "core/safe_path.hpp"
#include "scene/scene_config.hpp"

namespace redgem::render_detail {
namespace {

constexpr const char* kSkyExtensions[] = {"", ".hdr", ".png", ".jpg", ".jpeg"};
constexpr float kGamma = 2.2f;  // the curve sRGB pictures are stored with, as the shaders take it

float linear(unsigned char channel) {
  return std::pow(static_cast<float>(channel) / static_cast<float>(config::kMaxColorByte), kGamma);
}

Vec3 linear_pixel(const unsigned char* pixel) {
  return {linear(pixel[kXOffset]), linear(pixel[kYOffset]), linear(pixel[kZOffset])};
}

// An ordinary picture's pixels (8 bits a channel, sRGB) in linear light, shrunk as they are read
// so that a large picture never takes much memory.
FloatImage linear_picture(Image& decoded, std::uint32_t shrink) {
  ImageFormat(&decoded, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
  const auto width = static_cast<std::uint32_t>(decoded.width);
  FloatImage image = black_image(width / shrink, static_cast<std::uint32_t>(decoded.height) / shrink);
  const float share = 1.0f / static_cast<float>(shrink * shrink);
  const auto* bytes = static_cast<const unsigned char*>(decoded.data);
  for (std::uint32_t y = 0; y < image.height * shrink; ++y) {
    for (std::uint32_t x = 0; x < image.width * shrink; ++x) {
      add_share(image, x / shrink, y / shrink, linear_pixel(bytes + (y * width + x) * kChannelsPerPixel), share);
    }
  }
  return image;
}

// Whether a decoded picture is there and tall enough to be shrunk that much (a sky is twice as
// wide as it is tall: one only a few pixels high is not one).
bool fits_shrunk(const Image& decoded, std::uint32_t shrink) {
  return IsImageValid(decoded) && static_cast<std::uint32_t>(decoded.height) >= shrink;
}

std::optional<FloatImage> decode_ordinary(const std::string& extension, const std::vector<std::uint8_t>& encoded,
                                          std::uint32_t widest) {
  if (!decodable_image(encoded.data(), encoded.size())) return std::nullopt;
  Image decoded = LoadImageFromMemory(extension.c_str(), encoded.data(), static_cast<int>(encoded.size()));
  const std::uint32_t shrink = shrink_to_fit(static_cast<std::uint32_t>(decoded.width), widest);
  std::optional<FloatImage> image;
  if (fits_shrunk(decoded, shrink)) image = linear_picture(decoded, shrink);
  UnloadImage(decoded);
  return image;
}

std::optional<FloatImage> decode_sky(const std::filesystem::path& file, std::uint32_t widest) {
  const std::optional<std::vector<std::uint8_t>> encoded = read_file_up_to(file, kMaxEncodedImageBytes);
  if (!encoded) return std::nullopt;
  if (is_radiance(encoded->data(), encoded->size())) return decode_radiance(encoded->data(), encoded->size(), widest);
  return decode_ordinary(file.extension().string(), *encoded, widest);
}

}  // namespace

std::optional<FloatImage> load_sky_picture(const std::filesystem::path& folder, const std::string& name,
                                           std::uint32_t widest) {
  const std::optional<std::filesystem::path> file = find_asset_file(folder, name, kSkyExtensions);
  std::optional<FloatImage> picture = file ? decode_sky(*file, widest) : std::nullopt;
  if (!picture) {
    log::warning("no sky picture '" + printable(name) + "' that can be read in " + folder.string() +
                 ": a Radiance (.hdr), PNG or JPEG picture of at most " + std::to_string(kMaxImageSide) +
                 " pixels a side");
  }
  return picture;
}

}  // namespace redgem::render_detail
