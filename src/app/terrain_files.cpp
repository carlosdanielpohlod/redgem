#include "app/terrain_files.hpp"

#include <raylib.h>

#include <cstdint>
#include <system_error>
#include <vector>

#include "core/config.hpp"
#include "core/files.hpp"
#include "core/image_header.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "render/texture_loading.hpp"
#include "scene/ground/splat_map.hpp"
#include "scene/ground/terrain_layers_file.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

constexpr std::uintmax_t kMaxLayersFileBytes = 1024u * 1024u;
constexpr std::size_t kChannels = 4;
constexpr const char* kPngHint = ".png";

std::size_t splat_bytes(int size) {
  return static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * kChannels;
}

std::optional<nlohmann::json> layers_json(const std::filesystem::path& game_dir, const std::string& name,
                                          std::string& error) {
  const auto file = safe_asset_name(name) ? file_inside(game_dir, terrain_layers_path(name)) : std::nullopt;
  const auto bytes = file ? read_file_up_to(*file, kMaxLayersFileBytes) : std::nullopt;
  if (!bytes) {
    error = "there is no readable " + terrain_layers_path(name);
    return std::nullopt;
  }
  auto json = nlohmann::json::parse(bytes->begin(), bytes->end(), nullptr, false);
  if (json.is_discarded()) error = terrain_layers_path(name) + " is not JSON";
  return json.is_discarded() ? std::nullopt : std::optional<nlohmann::json>(std::move(json));
}

// One of the map's pictures, as RGBA8 of its size; nullopt when there is none (or it cannot be read).
std::optional<std::vector<std::uint8_t>> splat_pixels(const std::filesystem::path& game_dir, const std::string& name,
                                                      int picture, int size) {
  const auto file = file_inside(game_dir, splat_picture_path(name, picture));
  const auto encoded = file ? read_file_up_to(*file, kMaxEncodedImageBytes) : std::nullopt;
  if (!encoded) return std::nullopt;
  Image image = render_detail::decode_picture(*encoded);
  if (image.data == nullptr) {
    log::warning(splat_picture_path(name, picture) + " cannot be read: its layers start empty");
    return std::nullopt;
  }
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  if (image.width != size || image.height != size) ImageResize(&image, size, size);
  const auto* bytes = static_cast<const std::uint8_t*>(image.data);
  std::vector<std::uint8_t> pixels(bytes, bytes + splat_bytes(size));
  UnloadImage(image);
  return pixels;
}

// The weights beside the layers file: a map all of the first layer when there are none yet.
SplatMap read_splat(const std::filesystem::path& game_dir, const std::string& name, int size) {
  SplatMap splat = make_splat(size, 0);
  bool found = false;
  for (int picture = 0; picture < kSplatPictures; ++picture) {
    const auto pixels = splat_pixels(game_dir, name, picture, splat.size);
    if (!pixels) continue;
    if (!found) splat = SplatMap{splat.size, std::vector<std::uint8_t>(splat.weights.size(), 0), 0};
    found = take_splat_picture(splat, picture, *pixels) || found;
  }
  if (found) renormalize(splat);
  return splat;
}

bool uses_second_picture(const TerrainLayers& layers) {
  return layers.layers.size() > static_cast<std::size_t>(kLayersPerSplatPicture);
}

bool write_splat_picture(const std::filesystem::path& file, const std::vector<std::uint8_t>& rgba, int size,
                         std::string& error) {
  const Image image{const_cast<std::uint8_t*>(rgba.data()), size, size, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  int length = 0;
  unsigned char* png = ExportImageToMemory(image, kPngHint, &length);
  if (png == nullptr) {
    error = "cannot encode " + file.filename().string();
    return false;
  }
  const std::vector<std::uint8_t> bytes(png, png + length);
  MemFree(png);
  return write_bytes_file(file, bytes, error);
}

void remove_stale_picture(const std::filesystem::path& file) {
  std::error_code ignored;
  std::filesystem::remove(file, ignored);
}

}  // namespace

std::optional<TerrainLayers> read_terrain_files(const std::filesystem::path& game_dir, const std::string& name,
                                                TerrainReading reading, std::string& error) {
  const auto json = layers_json(game_dir, name, error);
  if (!json) return std::nullopt;
  TerrainLayers layers;
  layers.name = name;
  error = read_terrain_layers(*json, layers);
  if (!error.empty()) return std::nullopt;
  layers.splat = read_splat(game_dir, name, layers.splat_size);
  if (reading == TerrainReading::WeightsAndPictures) layers.pictures = pack_terrain_pictures(game_dir, layers);
  return layers;
}

void attach_terrain_layers(SceneDesc& scene, const std::filesystem::path& game_dir, TerrainReading reading) {
  scene.terrains.clear();
  for (std::size_t node = 0; node < scene.nodes.size(); ++node) {
    const auto name = get_string(scene.nodes[node].props, props::kTerrainLayers);
    if (!name || scene.nodes[node].mesh == kNoIndex) continue;
    std::string error;
    auto layers = read_terrain_files(game_dir, *name, reading, error);
    if (!layers) {
      log::warning("object '" + printable(scene.nodes[node].name) + "' keeps its own material: " + printable(error));
      continue;
    }
    layers->node = static_cast<int>(node);
    scene.terrains.push_back(std::move(*layers));
  }
}

const TerrainLayers* terrain_layers_of(const SceneDesc& scene, int node) {
  for (const TerrainLayers& layers : scene.terrains) {
    if (layers.node == node) return &layers;
  }
  return nullptr;
}

TerrainLayers* terrain_layers_of(SceneDesc& scene, int node) {
  return const_cast<TerrainLayers*>(terrain_layers_of(static_cast<const SceneDesc&>(scene), node));
}

bool write_splat_pictures(const std::filesystem::path& game_dir, const TerrainLayers& layers, std::string& error) {
  if (!safe_asset_name(layers.name) || !well_formed(layers.splat)) {
    error = "the terrain has no weights to write";
    return false;
  }
  std::error_code ignored;
  std::filesystem::create_directories(game_dir / config::kTerrainsDirName, ignored);
  const int pictures = uses_second_picture(layers) ? kSplatPictures : 1;
  for (int picture = 0; picture < pictures; ++picture) {
    const auto file = game_dir / splat_picture_path(layers.name, picture);
    if (!write_splat_picture(file, splat_picture(layers.splat, picture), layers.splat.size, error)) return false;
  }
  if (!uses_second_picture(layers)) remove_stale_picture(game_dir / splat_picture_path(layers.name, 1));
  return true;
}

}  // namespace redgem
