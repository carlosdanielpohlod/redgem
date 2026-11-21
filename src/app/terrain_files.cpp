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

bool uses_second_picture(const TerrainLayers& layers) {
  return layers.layers.size() > static_cast<std::size_t>(kLayersPerSplatPicture);
}

// One of the map's pictures, RGBA8.
struct SplatPixels {
  int size = 0;
  std::vector<std::uint8_t> rgba;
};

bool usable_splat_side(const Image& image) {
  return image.width == image.height && image.width >= kMinSplatSize && image.width <= kMaxSplatSize;
}

// A picture of the game's folder, decoded; nothing (no data) when it is not there.
Image decoded_file(const std::filesystem::path& game_dir, const std::string& relative) {
  const auto file = file_inside(game_dir, relative);
  const auto encoded = file ? read_file_up_to(*file, kMaxEncodedImageBytes) : std::nullopt;
  return encoded ? render_detail::decode_picture(*encoded) : Image{};
}

// One of the map's pictures; nullopt when there is none, or it cannot be used (warned of).
std::optional<SplatPixels> splat_pixels(const std::filesystem::path& game_dir, const std::string& name, int picture) {
  const std::string relative = splat_picture_path(name, picture);
  if (!file_inside(game_dir, relative)) return std::nullopt;
  Image image = decoded_file(game_dir, relative);
  if (image.data == nullptr || !usable_splat_side(image)) {
    log::warning(splat_picture_path(name, picture) +
                 " is not a square picture of 16 to 4096 pixels: its spots go to the first layer");
    UnloadImage(image);
    return std::nullopt;
  }
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  const auto* bytes = static_cast<const std::uint8_t*>(image.data);
  SplatPixels pixels{image.width, std::vector<std::uint8_t>(bytes, bytes + splat_bytes(image.width))};
  UnloadImage(image);
  return pixels;
}

// Takes a picture into the map read so far (made at the picture's size by the first one).
void take_into(std::optional<SplatMap>& read, int picture, const SplatPixels& pixels, const std::string& name) {
  if (!read) read = SplatMap{pixels.size, std::vector<std::uint8_t>(splat_bytes(pixels.size) * kSplatPictures, 0), 0};
  if (!take_splat_picture(*read, picture, pixels.rgba))
    log::warning(splat_picture_path(name, picture) + " is not the size of the other weights: it is left out");
}

// The weights beside the layers file, at its splat_size: all of the first layer when there are
// none yet. Only the pictures its layers need are read, and no weight is left on a layer it lost.
SplatMap read_splat(const std::filesystem::path& game_dir, const TerrainLayers& layers) {
  std::optional<SplatMap> read;
  const int pictures = uses_second_picture(layers) ? kSplatPictures : 1;
  for (int picture = 0; picture < pictures; ++picture) {
    const auto pixels = splat_pixels(game_dir, layers.name, picture);
    if (pixels) take_into(read, picture, *pixels, layers.name);
  }
  if (!read) return make_splat(layers.splat_size, 0);
  keep_layers(*read, static_cast<int>(layers.layers.size()));
  return read->size == layers.splat_size ? std::move(*read) : resampled(*read, layers.splat_size);
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

std::filesystem::path writing_path(const std::filesystem::path& file) {
  return file.string() + config::kSceneWritingSuffix;
}

// Writes the map's pictures beside where they go (see put_in_place), listing where they go.
bool write_aside(const std::filesystem::path& game_dir, const TerrainLayers& layers,
                 std::vector<std::filesystem::path>& files, std::string& error) {
  const int pictures = uses_second_picture(layers) ? kSplatPictures : 1;
  for (int picture = 0; picture < pictures; ++picture) {
    files.push_back(game_dir / splat_picture_path(layers.name, picture));
    const auto rgba = splat_picture(layers.splat, picture);
    if (!write_splat_picture(writing_path(files.back()), rgba, layers.splat.size, error)) return false;
  }
  return true;
}

// Moves the pictures written aside into place, once all of them are written.
bool put_in_place(const std::vector<std::filesystem::path>& files, std::string& error) {
  for (const auto& file : files) {
    std::error_code problem;
    std::filesystem::rename(writing_path(file), file, problem);
    if (problem) {
      error = "cannot replace " + file.filename().string() + ": " + problem.message();
      return false;
    }
  }
  return true;
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
  layers.splat = read_splat(game_dir, layers);
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
  std::vector<std::filesystem::path> files;
  if (!write_aside(game_dir, layers, files, error) || !put_in_place(files, error)) return false;
  if (!uses_second_picture(layers)) remove_stale_picture(game_dir / splat_picture_path(layers.name, 1));
  return true;
}

}  // namespace redgem
