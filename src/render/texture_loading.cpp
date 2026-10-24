#include "render/texture_loading.hpp"

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

#include "core/image_header.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "core/webp_image.hpp"

namespace redgem::render_detail {
namespace {

// How a picture's file may be named: as asked for, or with one of the kinds of picture read.
constexpr const char* kPictureExtensions[] = {"", ".png", ".jpg", ".jpeg", ".webp"};
constexpr int kMissingPictureSide = 64;  // a small, opaque diagnostic pattern
constexpr int kMissingPictureCell = 8;
constexpr ::Color kMissingPink{255, 0, 255, 255};
constexpr ::Color kMissingBlack{0, 0, 0, 255};

Texture2D missing_picture_texture() {
  Image checks = GenImageChecked(kMissingPictureSide, kMissingPictureSide, kMissingPictureCell, kMissingPictureCell,
                                 kMissingPink, kMissingBlack);
  const Texture2D texture = LoadTextureFromImage(checks);
  UnloadImage(checks);
  return texture;
}
Texture2D texture_or_placeholder(const Image& decoded) {
  return IsImageValid(decoded) ? LoadTextureFromImage(decoded) : missing_picture_texture();
}

constexpr int kRgbaChannels = 4;
Image decode_webp_picture(const EncodedImage& encoded) {
  const ImageSize size = encoded.dimensions();
  const int width = static_cast<int>(size.width);
  const int height = static_cast<int>(size.height);
  const auto capacity = static_cast<std::size_t>(width) * height * kRgbaChannels;
  // The decoder writes every pixel; use raylib's uninitialized allocator, paired with MemFree.
  auto* data = static_cast<std::uint8_t*>(RL_MALLOC(capacity));
  if (data == nullptr) return {};
  if (decode_webp_rgba_into(encoded, data, capacity, width * kRgbaChannels))
    return {data, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  MemFree(data);
  return {};
}

Image decode_picture_file(const std::filesystem::path& folder, const std::string& name) {
  const auto file = find_asset_file(folder, name, kPictureExtensions);
  if (!file) return {};
  const auto encoded = read_file_up_to(*file, kMaxEncodedImageBytes);
  if (!encoded) return {};
  return decode_picture(*encoded);
}

}  // namespace

// Caller owns the contextual warning; failed decoding here is silent.
Image decode_picture(const std::vector<std::uint8_t>& encoded) {
  const auto image = EncodedImage::read(encoded.data(), encoded.size());
  if (!image) return {};
  if (image->format() == EncodedImageFormat::Webp) return decode_webp_picture(*image);
  const char* hint = image->format() == EncodedImageFormat::Png ? ".png" : ".jpg";
  return LoadImageFromMemory(hint, encoded.data(), static_cast<int>(encoded.size()));
}

Texture2D load_scene_texture(const ImageData& image) {
  Image decoded = decode_picture(image.encoded);
  if (!IsImageValid(decoded)) log::warning("scene picture is unreadable; using a checkerboard");
  Texture2D texture = texture_or_placeholder(decoded);
  UnloadImage(decoded);
  if (texture.id == 0) return texture;
  GenTextureMipmaps(&texture);
  SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
  SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);  // floors stay sharp at grazing angles
  return texture;
}

Texture2D load_picture_file(const std::filesystem::path& folder, const std::string& name) {
  Image decoded = decode_picture_file(folder, name);
  if (!IsImageValid(decoded))
    log::warning("picture '" + printable(name) + "' is missing or unreadable; using a checkerboard");
  const Texture2D texture = texture_or_placeholder(decoded);
  UnloadImage(decoded);
  return texture;
}

}  // namespace redgem::render_detail
