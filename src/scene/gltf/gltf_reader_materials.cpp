// Pictures and materials of a glTF file.

#include <algorithm>
#include <cmath>

#include "core/ktx2_image.hpp"
#include "core/safe_path.hpp"
#include "scene/gltf/extras_parser.hpp"
#include "scene/props.hpp"
#include "scene/gltf/gltf_checks.hpp"
#include "scene/gltf/gltf_reader.hpp"

namespace redgem {
namespace {

// The file kind each image's mime type names (the decoder's hint); PNG for any other.
struct ImageKind {
  const char* mime_type;
  const char* extension;
};
constexpr ImageKind kImageKinds[] = {{"image/jpeg", ".jpg"}, {"image/webp", ".webp"}, {"image/ktx2", ".ktx2"}};
constexpr const char* kPngExtension = ".png";
const char* image_extension(const cgltf_image& image) {
  const std::string mime = text_or(image.mime_type, "");
  for (const ImageKind& kind : kImageKinds) {
    if (mime == kind.mime_type) return kind.extension;
  }
  return kPngExtension;
}
bool has_basisu_image(const cgltf_texture& texture) { return texture.has_basisu && texture.basisu_image != nullptr; }
bool has_webp_image(const cgltf_texture& texture) { return texture.has_webp && texture.webp_image != nullptr; }
// What a texture shows besides its KTX2 picture: its WebP one (EXT_texture_webp), or its plain one.
const cgltf_image* fallback_image(const cgltf_texture& texture) {
  if (has_webp_image(texture)) return texture.webp_image;
  return texture.image;
}

// Whether the scene's image `image` (a number, perhaps kNoIndex) holds a KTX2 picture the engine
// reads: one the transcoder takes, not only one whose header passes (newer Basis formats do).
bool readable_ktx2(const std::vector<ImageData>& images, int image) {
  if (!is_index_of(image, images.size())) return false;
  const std::vector<std::uint8_t>& encoded = images[static_cast<std::size_t>(image)].encoded;
  return opens_ktx2(encoded.data(), encoded.size());
}

// A texture shows its KTX2 picture (KHR_texture_basisu) when the engine reads it, or when it has
// nothing else to show; otherwise its fallback, as the extension asks of a reader that cannot.
bool shows_ktx2(const std::vector<ImageData>& images, int ktx2, const cgltf_texture& texture) {
  return readable_ktx2(images, ktx2) || fallback_image(texture) == nullptr;
}

std::vector<std::uint8_t> embedded_bytes(const cgltf_buffer_view& view) {
  const auto* bytes = static_cast<const std::uint8_t*>(cgltf_buffer_view_data(&view));
  if (bytes == nullptr) return {};
  return {bytes, bytes + view.size};
}

AlphaMode alpha_mode_of(cgltf_alpha_mode mode) {
  switch (mode) {
    case cgltf_alpha_mode_mask:
      return AlphaMode::Mask;
    case cgltf_alpha_mode_blend:
      return AlphaMode::Blend;
    default:
      return AlphaMode::Opaque;
  }
}

// Linear RGB, already multiplied by the emissive strength.
Vec3 emissive_of(const cgltf_material& material) {
  const float strength = material.has_emissive_strength ? material.emissive_strength.emissive_strength : 1.0f;
  return vec3_from(material.emissive_factor) * strength;
}

// What a material's custom properties say of how it is see-through.
bool dithered(const cgltf_material& material) {
  if (material.extras.data == nullptr) return false;
  const ExtrasParseResult parsed = parse_extras(material.extras.data);
  return get_string(parsed.properties, props::kTransparency).value_or("") == props::kDithered;
}

// Limits also keep malformed custom properties from creating invalid shader arithmetic.
constexpr double kMaxWaveHeight = 10.0;
constexpr double kMinWavelength = 0.05;
constexpr double kMaxWavelength = 1000.0;
constexpr double kMaxWaveSpeed = 10.0;
constexpr double kMaxAbsorption = 10.0;  // inverse meters
constexpr double kMaxFoamWidth = 10.0;
constexpr double kMaxRefraction = 0.1;

float water_number(const PropertyMap& properties, const char* key, float fallback, double low, double high) {
  const double value = get_number(properties, key).value_or(fallback);
  return std::isfinite(value) ? static_cast<float>(std::clamp(value, low, high)) : fallback;
}

WaterDesc water_of(const cgltf_material& material) {
  WaterDesc water;
  if (material.extras.data == nullptr) return water;
  const PropertyMap properties = parse_extras(material.extras.data).properties;
  water.enabled = get_bool(properties, props::kWater).value_or(false);
  water.wave_height = water_number(properties, props::kWaveHeight, water.wave_height, 0.0, kMaxWaveHeight);
  water.wavelength = water_number(properties, props::kWavelength, water.wavelength, kMinWavelength, kMaxWavelength);
  water.speed = water_number(properties, props::kWaveSpeed, water.speed, 0.0, kMaxWaveSpeed);
  water.foam_width = water_number(properties, props::kFoamWidth, water.foam_width, 0.0, kMaxFoamWidth);
  water.refraction = water_number(properties, props::kRefraction, water.refraction, 0.0, kMaxRefraction);
  water.absorption.x = water_number(properties, props::kAbsorptionRed, water.absorption.x, 0.0, kMaxAbsorption);
  water.absorption.y = water_number(properties, props::kAbsorptionGreen, water.absorption.y, 0.0, kMaxAbsorption);
  water.absorption.z = water_number(properties, props::kAbsorptionBlue, water.absorption.z, 0.0, kMaxAbsorption);
  return water;
}

}  // namespace

ImageData GltfReader::load_image(const cgltf_image& image, const std::filesystem::path& file) const {
  ImageData out;
  out.file_extension = image_extension(image);
  if (image.buffer_view != nullptr) {
    out.encoded = embedded_bytes(*image.buffer_view);
  } else if (!file.empty()) {
    // (Only a picture file of the scene's own folder, and not one of any size.)
    out.encoded = read_file_up_to(file, gltf_checks::kMaxImageFileBytes).value_or(std::vector<std::uint8_t>());
    if (file.has_extension()) out.file_extension = file.extension().string();
  }
  return out;
}

void GltfReader::load_images() {
  for (cgltf_size i = 0; i < data_.images_count; ++i) {
    ImageData image = load_image(data_.images[i], image_files_[i]);
    if (image.encoded.empty()) warn("image " + std::to_string(i) + " has no readable data");
    result_.scene.images.push_back(std::move(image));
  }
}

int GltfReader::image_of(const cgltf_texture_view& view) const {
  if (view.texture == nullptr) return kNoIndex;
  const cgltf_texture& texture = *view.texture;
  const int ktx2 = index_of(has_basisu_image(texture) ? texture.basisu_image : nullptr, data_.images);
  if (shows_ktx2(result_.scene.images, ktx2, texture)) return ktx2;
  return index_of(fallback_image(texture), data_.images);
}

void GltfReader::read_pbr(const cgltf_material& material, MaterialDesc& out) const {
  if (!material.has_pbr_metallic_roughness) return;
  const auto& pbr = material.pbr_metallic_roughness;
  const float* color = pbr.base_color_factor;
  out.base_color = {color[kRedChannel], color[kGreenChannel], color[kBlueChannel], color[kAlphaChannel]};
  out.metallic = pbr.metallic_factor;
  out.roughness = pbr.roughness_factor;
  out.base_color_image = image_of(pbr.base_color_texture);
  out.metallic_roughness_image = image_of(pbr.metallic_roughness_texture);
}

void GltfReader::read_clearcoat(const cgltf_material& material, MaterialDesc& out) const {
  if (!material.has_clearcoat) return;
  const auto& coat = material.clearcoat;
  out.clearcoat.factor = coat.clearcoat_factor;
  out.clearcoat.roughness = coat.clearcoat_roughness_factor;
  out.clearcoat.factor_image = image_of(coat.clearcoat_texture);
  out.clearcoat.roughness_image = image_of(coat.clearcoat_roughness_texture);
  out.clearcoat.normal_image = image_of(coat.clearcoat_normal_texture);
  if (out.clearcoat.normal_image != kNoIndex) out.clearcoat.normal_scale = coat.clearcoat_normal_texture.scale;
}

void GltfReader::read_detail_textures(const cgltf_material& material, MaterialDesc& out) const {
  out.normal_image = image_of(material.normal_texture);
  if (out.normal_image != kNoIndex) out.normal_scale = material.normal_texture.scale;
  out.occlusion_image = image_of(material.occlusion_texture);
  if (out.occlusion_image != kNoIndex) out.occlusion_strength = material.occlusion_texture.scale;
  out.emissive_image = image_of(material.emissive_texture);
}

MaterialDesc GltfReader::load_material(const cgltf_material& material) const {
  MaterialDesc out;
  out.name = text_or(material.name, "");
  read_pbr(material, out);
  read_detail_textures(material, out);
  read_clearcoat(material, out);
  out.water = water_of(material);
  out.emissive = emissive_of(material);
  out.alpha_mode = alpha_mode_of(material.alpha_mode);
  out.alpha_cutoff = material.alpha_cutoff;
  out.double_sided = material.double_sided;
  out.dithered = dithered(material);
  return out;
}

void GltfReader::load_materials() {
  for (cgltf_size i = 0; i < data_.materials_count; ++i) {
    result_.scene.materials.push_back(load_material(data_.materials[i]));
  }
}

}  // namespace redgem
