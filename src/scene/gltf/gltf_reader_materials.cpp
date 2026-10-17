// Pictures and materials of a glTF file.

#include "core/safe_path.hpp"
#include "scene/gltf/extras_parser.hpp"
#include "scene/props.hpp"
#include "scene/gltf/gltf_checks.hpp"
#include "scene/gltf/gltf_reader.hpp"

namespace redgem {
namespace {

constexpr const char* kJpegMimeType = "image/jpeg";
constexpr const char* kJpegExtension = ".jpg";
constexpr const char* kPngExtension = ".png";

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

}  // namespace

ImageData GltfReader::load_image(const cgltf_image& image, const std::filesystem::path& file) const {
  ImageData out;
  out.file_extension = text_or(image.mime_type, "") == kJpegMimeType ? kJpegExtension : kPngExtension;
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
  // KHR_texture_basisu / webp images are not decodable here; fall back to the plain image.
  return index_of(view.texture->image, data_.images);
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
