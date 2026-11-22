#include "render/texture_loading.hpp"

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

#include "core/image_header.hpp"
#include "core/import_settings.hpp"
#include "core/ktx2_header.hpp"
#include "core/ktx2_image.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "core/webp_image.hpp"
#include "render/compressed_textures.hpp"

namespace redgem::render_detail {
namespace {

// How a picture's file may be named: as asked for, or with one of the kinds of picture read.
constexpr const char* kPictureExtensions[] = {"", ".png", ".jpg", ".jpeg", ".webp", ".ktx2"};
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

// A KTX2 picture's full-size level as RGBA8, transcoded straight into memory raylib owns (MemFree,
// as UnloadImage does).
Image decode_ktx2_picture(const std::vector<std::uint8_t>& encoded) {
  const std::optional<Ktx2Header> header = read_ktx2_header(encoded.data(), encoded.size());
  if (!header) return {};
  const int width = static_cast<int>(header->size.width);
  const int height = static_cast<int>(header->size.height);
  const auto capacity = static_cast<std::size_t>(width) * height * kRgbaChannels;
  auto* data = static_cast<std::uint8_t*>(RL_MALLOC(capacity));
  if (data == nullptr) return {};
  if (decode_ktx2_rgba_into(encoded.data(), encoded.size(), data, capacity))
    return {data, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  MemFree(data);
  return {};
}

// Whether a picture is a KTX2 one that brought its own mipmaps (what the card may take compressed).
bool brings_mipmaps(const std::vector<std::uint8_t>& encoded) {
  const std::optional<Ktx2Header> header = read_ktx2_header(encoded.data(), encoded.size());
  return header && header->levels > 1;
}

// Filtered so that floors stay sharp at grazing angles.
Texture2D filtered(Texture2D texture) {
  if (texture.id == 0) return texture;
  SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
  SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);
  return texture;
}

// With the smaller copies the card makes for it, filtered.
Texture2D with_smaller_copies(Texture2D texture) {
  if (texture.id != 0) GenTextureMipmaps(&texture);
  return filtered(texture);
}

// A KTX2 picture that brought its own mipmaps, on a card that takes a compressed format: it stays
// compressed there. (A single level would get no mipmaps: the card makes none for compressed
// textures, so that picture goes up decoded, as any other.)
bool goes_up_compressed(const std::vector<std::uint8_t>& encoded) {
  return brings_mipmaps(encoded) && is_block_compressed(best_block_format(gpu_texture_support(), false));
}

// The texture of a KTX2 picture kept compressed; an empty one (id 0) for any other picture.
Texture2D compressed_scene_texture(const std::vector<std::uint8_t>& encoded) {
  if (!goes_up_compressed(encoded)) return Texture2D{};
  const std::optional<TranscodedPicture> picture =
      transcode_ktx2(encoded.data(), encoded.size(), gpu_texture_support());
  return picture ? upload_compressed(*picture) : Texture2D{};
}

// The texture of a picture decoded on the CPU, with the mipmaps the card makes for it.
Texture2D decoded_scene_texture(const ImageData& image) {
  Image decoded = decode_picture(image.encoded);
  if (!IsImageValid(decoded)) log::warning("scene picture is unreadable; using a checkerboard");
  Texture2D texture = texture_or_placeholder(decoded);
  UnloadImage(decoded);
  if (texture.id != 0) GenTextureMipmaps(&texture);
  return texture;
}

Image decode_picture_file(const std::filesystem::path& file) {
  const auto encoded = read_file_up_to(file, kMaxEncodedImageBytes);
  if (!encoded) return {};
  Image decoded = decode_picture(*encoded);
  if (IsImageValid(decoded)) fit_picture(decoded, import_settings::texture_settings(file).max_size);
  return decoded;
}

}  // namespace

// Caller owns the contextual warning; failed decoding here is silent.
Image decode_picture(const std::vector<std::uint8_t>& encoded) {
  if (starts_as_ktx2(encoded.data(), encoded.size())) return decode_ktx2_picture(encoded);
  const auto image = EncodedImage::read(encoded.data(), encoded.size());
  if (!image) return {};
  if (image->format() == EncodedImageFormat::Webp) return decode_webp_picture(*image);
  const char* hint = image->format() == EncodedImageFormat::Png ? ".png" : ".jpg";
  return LoadImageFromMemory(hint, encoded.data(), static_cast<int>(encoded.size()));
}

Texture2D load_scene_texture(const ImageData& image) {
  Texture2D texture = compressed_scene_texture(image.encoded);
  if (texture.id == 0) texture = decoded_scene_texture(image);
  return filtered(texture);
}

Texture2D load_scene_texture(const ImageData& image, const DecodedPicture& decoded) {
  if (decoded.unreadable) {
    log::warning("scene picture is unreadable; using a checkerboard");
    return with_smaller_copies(missing_picture_texture());
  }
  if (decoded.empty()) return load_scene_texture(image);
  // (raylib only reads the pixels of an image it uploads.)
  const Image rows{const_cast<std::uint8_t*>(decoded.pixels.data()), decoded.width, decoded.height, 1, decoded.format};
  return with_smaller_copies(LoadTextureFromImage(rows));
}

DecodedPicture decode_scene_picture(const ImageData& image) {
  // (One that may stay compressed on the card is left to the thread that knows the card.)
  if (brings_mipmaps(image.encoded)) return {};
  Image decoded = decode_picture(image.encoded);
  if (!IsImageValid(decoded)) return DecodedPicture{0, 0, 0, {}, true};
  const auto* first = static_cast<const std::uint8_t*>(decoded.data);
  const auto bytes = static_cast<std::size_t>(GetPixelDataSize(decoded.width, decoded.height, decoded.format));
  DecodedPicture picture{decoded.width, decoded.height, decoded.format, {first, first + bytes}, false};
  UnloadImage(decoded);
  return picture;
}

std::optional<std::filesystem::path> picture_file(const std::filesystem::path& folder, const std::string& name) {
  return find_asset_file(folder, name, kPictureExtensions);
}

void fit_picture(Image& image, int max_size) {
  const import_settings::PictureSize size{image.width, image.height};
  const import_settings::PictureSize fitted = import_settings::fitted_size(size, max_size);
  if (!(fitted == size)) ImageResize(&image, fitted.width, fitted.height);
}

Texture2D load_picture_file(const std::filesystem::path& folder, const std::string& name) {
  const auto file = picture_file(folder, name);
  Image decoded = file ? decode_picture_file(*file) : Image{};
  if (!IsImageValid(decoded))
    log::warning("picture '" + printable(name) + "' is missing or unreadable; using a checkerboard");
  const Texture2D texture = texture_or_placeholder(decoded);
  UnloadImage(decoded);
  return texture;
}

}  // namespace redgem::render_detail
