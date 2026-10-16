#include "render/texture_loading.hpp"

#include <cstdint>
#include <optional>
#include <vector>

#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "core/image_header.hpp"

namespace redgem::render_detail {
namespace {

// How a picture's file may be named: as asked for, or with one of the kinds of picture read.
constexpr const char* kPictureExtensions[] = {"", ".png", ".jpg", ".jpeg"};

// An encoded picture, decoded. An empty image (and a warning) for one that is not a PNG or JPEG
// within the size limits: its header is read first, so that nothing oversized is ever decoded.
Image decode_picture(const std::string& file_extension, const std::vector<std::uint8_t>& encoded) {
  if (!decodable_image(encoded.data(), encoded.size())) {
    log::warning("a picture is left out: it is not a PNG or JPEG of at most " + std::to_string(kMaxImageSide) + " x " +
                 std::to_string(kMaxImageSide) + " pixels");
    return Image{};
  }
  return LoadImageFromMemory(file_extension.c_str(), encoded.data(), static_cast<int>(encoded.size()));
}

}  // namespace

Texture2D load_scene_texture(const ImageData& image) {
  Texture2D texture{};
  if (image.encoded.empty()) return texture;
  Image decoded = decode_picture(image.file_extension, image.encoded);
  if (IsImageValid(decoded)) {
    texture = LoadTextureFromImage(decoded);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);       // smooth, and it is what makes the mipmaps used
    SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);  // floors stay sharp at grazing angles
  }
  UnloadImage(decoded);
  return texture;
}

Texture2D load_picture_file(const std::filesystem::path& folder, const std::string& name) {
  const std::optional<std::filesystem::path> file = find_asset_file(folder, name, kPictureExtensions);
  if (!file) return Texture2D{};
  const std::optional<std::vector<std::uint8_t>> encoded = read_file_up_to(*file, kMaxEncodedImageBytes);
  if (!encoded) return Texture2D{};
  Image decoded = decode_picture(file->extension().string(), *encoded);
  const Texture2D texture = IsImageValid(decoded) ? LoadTextureFromImage(decoded) : Texture2D{};
  UnloadImage(decoded);
  return texture;
}

}  // namespace redgem::render_detail
