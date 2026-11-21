// A terrain's layers' pictures, read from the game's folder and packed the way the renderer takes
// them (see TerrainPictures): every layer the same size, four channels a picture.

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "app/terrain_files.hpp"
#include "core/image_header.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "render/texture_loading.hpp"
#include "scene/color_space.hpp"

namespace redgem {
namespace {

constexpr std::size_t kChannels = 4;  // RGBA
constexpr std::size_t kRed = 0;
constexpr std::size_t kGreen = 1;
constexpr std::size_t kBlue = 2;
constexpr std::size_t kAlpha = 3;
constexpr float kFullByte = 255.0f;
constexpr float kMiddleByte = 127.5f;      // 0 in a normal map's -1..1
constexpr std::uint8_t kNoHeight = 128;    // a layer without a height picture: halfway up
constexpr std::uint8_t kFlatNormal = 128;  // a layer without a normal map faces straight out
constexpr std::uint8_t kFullByteValue = 255;

using Pixels = std::vector<std::uint8_t>;

// A picture of the game's folder, decoded, RGBA8, `side` x `side`; nullopt for "" or a picture
// that cannot be read (warned of).
std::optional<Pixels> picture_pixels(const std::filesystem::path& game_dir, const std::string& relative, int side) {
  if (relative.empty()) return std::nullopt;
  const auto file = file_inside(game_dir, relative);
  const auto encoded = file ? read_file_up_to(*file, kMaxEncodedImageBytes) : std::nullopt;
  Image image = encoded ? render_detail::decode_picture(*encoded) : Image{};
  if (image.data == nullptr) {
    log::warning("terrain picture '" + printable(relative) + "' cannot be read: the layer goes without it");
    return std::nullopt;
  }
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  ImageResize(&image, side, side);
  const auto* bytes = static_cast<const std::uint8_t*>(image.data);
  Pixels pixels(bytes, bytes + static_cast<std::size_t>(side) * static_cast<std::size_t>(side) * kChannels);
  UnloadImage(image);
  return pixels;
}

std::uint8_t byte_of(float value) { return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, kFullByte))); }

// What a layer's pictures say at one of its pixels, read from each (or what it has without one).
struct LayerSources {
  std::optional<Pixels> albedo, normal, roughness, occlusion, height;
};

std::uint8_t channel_or(const std::optional<Pixels>& pixels, std::size_t at, std::uint8_t missing) {
  return pixels ? (*pixels)[at] : missing;
}

void pack_color(const LayerSources& sources, const TerrainLayer& layer, std::size_t at, std::uint8_t* out) {
  // (A tint times the picture in linear light is the same as its sRGB value times the picture's.)
  const float tint[] = {srgb_from_linear(layer.tint.x), srgb_from_linear(layer.tint.y), srgb_from_linear(layer.tint.z)};
  for (std::size_t channel = kRed; channel <= kBlue; ++channel) {
    out[channel] =
        byte_of(static_cast<float>(channel_or(sources.albedo, at + channel, kFullByteValue)) * tint[channel]);
  }
  out[kAlpha] = channel_or(sources.height, at + kRed, kNoHeight);
}

std::uint8_t scaled_normal(const LayerSources& sources, std::size_t at, float scale) {
  const float stored = static_cast<float>(channel_or(sources.normal, at, kFlatNormal));
  return byte_of(kMiddleByte + (stored - kMiddleByte) * scale);
}

void pack_surface(const LayerSources& sources, const TerrainLayer& layer, std::size_t at, std::uint8_t* out) {
  out[kRed] = scaled_normal(sources, at + kRed, layer.normal_scale);
  out[kGreen] = scaled_normal(sources, at + kGreen, layer.normal_scale);
  out[kBlue] =
      byte_of(static_cast<float>(channel_or(sources.roughness, at + kRed, kFullByteValue)) * layer.roughness_scale);
  out[kAlpha] = channel_or(sources.occlusion, at + kRed, kFullByteValue);
}

LayerSources read_sources(const std::filesystem::path& game_dir, const TerrainLayer& layer, int side) {
  return {picture_pixels(game_dir, layer.albedo, side), picture_pixels(game_dir, layer.normal, side),
          picture_pixels(game_dir, layer.roughness, side), picture_pixels(game_dir, layer.occlusion, side),
          picture_pixels(game_dir, layer.height, side)};
}

void pack_layer(const std::filesystem::path& game_dir, const TerrainLayer& layer, std::size_t index,
                TerrainPictures& pictures) {
  const LayerSources sources = read_sources(game_dir, layer, pictures.side);
  const std::size_t pixels = static_cast<std::size_t>(pictures.side) * static_cast<std::size_t>(pictures.side);
  const std::size_t first = index * pixels * kChannels;
  for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
    pack_color(sources, layer, pixel * kChannels, &pictures.color[first + pixel * kChannels]);
    pack_surface(sources, layer, pixel * kChannels, &pictures.surface[first + pixel * kChannels]);
  }
  pictures.has_height[index] = sources.height.has_value();
}

}  // namespace

TerrainPictures pack_terrain_pictures(const std::filesystem::path& game_dir, const TerrainLayers& layers) {
  TerrainPictures pictures;
  pictures.side = layers.picture_size;
  pictures.layers = static_cast<int>(layers.layers.size());
  const std::size_t bytes = static_cast<std::size_t>(pictures.side) * static_cast<std::size_t>(pictures.side) *
                            kChannels * layers.layers.size();
  pictures.color.resize(bytes);
  pictures.surface.resize(bytes);
  for (std::size_t index = 0; index < layers.layers.size(); ++index)
    pack_layer(game_dir, layers.layers[index], index, pictures);
  return pictures;
}

}  // namespace redgem
