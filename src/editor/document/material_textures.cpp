#include "editor/document/material_textures.hpp"

#include <optional>

#include "editor/document/gltf_json.hpp"

namespace redgem::material_textures {
namespace {

using namespace gltf_json;
using Json = nlohmann::json;

constexpr const char* kBaseColorTexture = "baseColorTexture";
constexpr const char* kMetallicRoughnessTexture = "metallicRoughnessTexture";
constexpr const char* kNormalTexture = "normalTexture";
constexpr const char* kOcclusionTexture = "occlusionTexture";
constexpr const char* kEmissiveTexture = "emissiveTexture";
constexpr const char* kWebpExtension = "EXT_texture_webp";
constexpr const char* kMimeType = "mimeType";
constexpr const char* kWebpMime = "image/webp";
constexpr std::size_t kCoatSlots = 3;  // a clearcoat's factor, roughness and normal pictures

// Where a slot is in a material's JSON: under its "pbrMetallicRoughness" or at its top.
struct SlotPlace {
  bool in_pbr;
  const char* key;
};

SlotPlace place_of(Slot slot) {
  switch (slot) {
    case Slot::BaseColor:
      return {true, kBaseColorTexture};
    case Slot::MetallicRoughness:
      return {true, kMetallicRoughnessTexture};
    case Slot::Normal:
      return {false, kNormalTexture};
    case Slot::Occlusion:
      return {false, kOcclusionTexture};
    case Slot::Emissive:
      break;
  }
  return {false, kEmissiveTexture};
}

// The object a slot is kept in (the material, or its "pbrMetallicRoughness"); nullptr when the
// material has no such object.
const Json* holder_of(const Json& material, const SlotPlace& place) {
  if (!place.in_pbr) return &material;
  const auto pbr = material.find(kPbr);
  return pbr != material.end() && pbr->is_object() ? &*pbr : nullptr;
}

// What a texture or extension says its picture is ("source"), when it says it with an index.
std::optional<int> source_of(const Json& holder) {
  if (!holder.is_object() || !holder.contains(kSource)) return std::nullopt;
  return index_from(holder[kSource]);
}

// The WebP picture of a texture (EXT_texture_webp), when it has one.
std::optional<int> webp_source(const Json& texture) {
  if (!has_object(texture, kExtensions)) return std::nullopt;
  const auto webp = texture[kExtensions].find(kWebpExtension);
  return webp == texture[kExtensions].end() ? std::nullopt : source_of(*webp);
}

bool is_webp(const Json& doc, int image) {
  const Json* entry = entry_at(doc, kImages, image);
  if (entry == nullptr) return false;
  const auto mime = entry->find(kMimeType);
  return mime != entry->end() && *mime == kWebpMime;
}

Json texture_for(const Json& doc, int image) {
  if (!is_webp(doc, image)) return {{kSource, image}};
  return {{kExtensions, {{kWebpExtension, {{kSource, image}}}}}};
}

// The slot's JSON pointed at the texture: its other fields (scale, texture coordinates, extensions)
// stay when it had them.
void point_slot(Json& holder, const char* key, int texture) {
  if (!holder[key].is_object()) holder[key] = Json::object();
  holder[key][kIndex] = texture;
}

// Which field of a material description holds a slot's image.
using ImageField = int MaterialDesc::*;
ImageField field_of(Slot slot) {
  switch (slot) {
    case Slot::BaseColor:
      return &MaterialDesc::base_color_image;
    case Slot::MetallicRoughness:
      return &MaterialDesc::metallic_roughness_image;
    case Slot::Normal:
      return &MaterialDesc::normal_image;
    case Slot::Occlusion:
      return &MaterialDesc::occlusion_image;
    case Slot::Emissive:
      break;
  }
  return &MaterialDesc::emissive_image;
}

}  // namespace

int image_in(const MaterialDesc& material, Slot slot) { return material.*field_of(slot); }

int& image_in(MaterialDesc& material, Slot slot) { return material.*field_of(slot); }

std::vector<int> images_of(const MaterialDesc& material) {
  std::vector<int> images;
  images.reserve(kSlots.size() + kCoatSlots);
  for (const Slot slot : kSlots) images.push_back(image_in(material, slot));
  const ClearcoatDesc& coat = material.clearcoat;
  images.insert(images.end(), {coat.factor_image, coat.roughness_image, coat.normal_image});
  return images;
}

int image_of_texture(const Json& doc, int texture) {
  const Json* entry = entry_at(doc, kTextures, texture);
  if (entry == nullptr) return kNoIndex;
  return webp_source(*entry).value_or(source_of(*entry).value_or(kNoIndex));
}

int slot_image(const Json& doc, const Json& material, Slot slot) {
  const SlotPlace place = place_of(slot);
  const Json* holder = holder_of(material, place);
  if (holder == nullptr || !has_object(*holder, place.key)) return kNoIndex;
  const auto texture = (*holder)[place.key].find(kIndex);
  if (texture == (*holder)[place.key].end()) return kNoIndex;
  return image_of_texture(doc, index_from(*texture).value_or(kNoIndex));
}

int texture_showing(Json& doc, int image) {
  const int textures = has_list(doc, kTextures) ? static_cast<int>(doc[kTextures].size()) : 0;
  for (int texture = 0; texture < textures; ++texture) {
    if (image_of_texture(doc, texture) == image) return texture;
  }
  if (is_webp(doc, image)) add_extension_used(doc, kWebpExtension);
  array_at(doc, kTextures).push_back(texture_for(doc, image));
  return textures;
}

void write_slots(Json& doc, Json& material, const MaterialDesc& desc) {
  for (const Slot slot : kSlots) {
    const int image = image_in(desc, slot);
    if (slot_image(doc, material, slot) == image) continue;
    const SlotPlace place = place_of(slot);
    Json& holder = place.in_pbr ? material[kPbr] : material;
    if (image == kNoIndex) {
      holder.erase(place.key);
    } else {
      point_slot(holder, place.key, texture_showing(doc, image));
    }
  }
}

}  // namespace redgem::material_textures
