#include "render/texture_loading.hpp"

#include <cstdint>
#include <optional>
#include <vector>

#include "core/image_header.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"

namespace redgem::render_detail {
namespace {

// How a picture's file may be named: as asked for, or with one of the kinds of picture read.
constexpr const char* kPictureExtensions[] = {"", ".png", ".jpg", ".jpeg"};
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

// An encoded picture, decoded. An empty image (and a warning) for one that is not a PNG or JPEG
// within the size limits: its header is read first, so that nothing oversized is ever decoded.
Image decode_picture(const std::string& file_extension, const std::vector<std::uint8_t>& encoded) {
  if (!decodable_image(encoded.data(), encoded.size())) {
    log::warning("a picture cannot be decoded: expected a PNG or JPEG of at most " + std::to_string(kMaxImageSide) +
                 " x " + std::to_string(kMaxImageSide) + " pixels");
    return Image{};
  }
  return LoadImageFromMemory(file_extension.c_str(), encoded.data(), static_cast<int>(encoded.size()));
}

Image decode_picture_file(const std::filesystem::path& folder, const std::string& name) {
  const auto file = find_asset_file(folder, name, kPictureExtensions);
  if (!file) return {};
  const auto encoded = read_file_up_to(*file, kMaxEncodedImageBytes);
  if (!encoded) return {};
  return decode_picture(file->extension().string(), *encoded);
}

}  // namespace

Texture2D load_scene_texture(const ImageData& image) {
  Image decoded = decode_picture(image.file_extension, image.encoded);
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
