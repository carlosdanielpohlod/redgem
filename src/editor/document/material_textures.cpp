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
  return has_object(material, kPbr) ? &material[kPbr] : nullptr;
}

// Whether a JSON value is an object with `key`.
bool has_key(const Json& value, const char* key) { return value.is_object() && value.contains(key); }

// What a texture or extension says its picture is ("source"), when it says it with an index.
std::optional<int> source_of(const Json& holder) {
  if (!has_key(holder, kSource)) return std::nullopt;
  return index_from(holder[kSource]);
}

// The WebP picture of a texture (EXT_texture_webp), when it has one.
std::optional<int> webp_source(const Json& texture) {
  if (!has_object(texture, kExtensions)) return std::nullopt;
  const auto webp = texture[kExtensions].find(kWebpTextureExtension);
  return webp == texture[kExtensions].end() ? std::nullopt : source_of(*webp);
}

bool is_webp(const Json& doc, int image) {
  const Json* entry = entry_at(doc, kImages, image);
  return entry != nullptr && text_at(*entry, kMimeType) == kWebpMime;
}

Json texture_for(const Json& doc, int image) {
  if (!is_webp(doc, image)) return {{kSource, image}};
  return {{kExtensions, {{kWebpTextureExtension, {{kSource, image}}}}}};
}

// A texture that samples its picture the default way: no sampler, or one that says nothing.
// Another material's texture with a sampler of its own is not borrowed.
bool plain_sampling(const Json& doc, int texture) {
  const Json& entry = doc[kTextures][texture];
  if (!entry.contains(kSampler)) return true;
  const Json* sampler = entry_at(doc, kSamplers, entry[kSampler]);
  return sampler != nullptr && sampler->empty();
}

// A texture of the document can show `image` for a material set in the editor.
bool reusable(const Json& doc, int texture, int image) {
  return image_of_texture(doc, texture) == image && plain_sampling(doc, texture);
}

// Whether a texture shows its picture through EXT_texture_webp.
bool shows_webp(const Json& texture) {
  return has_object(texture, kExtensions) && texture[kExtensions].contains(kWebpTextureExtension);
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

// The holder of a material's slot has the slot.
bool holds_slot(const Json* holder, const char* key) { return holder != nullptr && has_object(*holder, key); }

// The object a slot is written into: the material, or its "pbrMetallicRoughness" (made one when
// the file holds something else there).
Json& writable_holder(Json& material, const SlotPlace& place) {
  if (!place.in_pbr) return material;
  if (!material[kPbr].is_object()) material[kPbr] = Json::object();
  return material[kPbr];
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
  if (!holds_slot(holder, place.key)) return kNoIndex;
  const auto texture = (*holder)[place.key].find(kIndex);
  if (texture == (*holder)[place.key].end()) return kNoIndex;
  return image_of_texture(doc, index_from(*texture).value_or(kNoIndex));
}

int texture_showing(Json& doc, int image) {
  const int textures = has_list(doc, kTextures) ? static_cast<int>(doc[kTextures].size()) : 0;
  for (int texture = 0; texture < textures; ++texture) {
    if (reusable(doc, texture, image)) return texture;
  }
  // (Without a picture another reader can show, a WebP one is required to read the file.)
  if (is_webp(doc, image)) add_extension_required(doc, kWebpTextureExtension);
  array_at(doc, kTextures).push_back(texture_for(doc, image));
  return textures;
}

void forget_unused_webp(Json& doc) {
  for (const Json& texture : list_of(doc, kTextures)) {
    if (shows_webp(texture)) return;
  }
  remove_extension_used(doc, kWebpTextureExtension);
}

// Points one slot of a material's JSON at `image`, or takes it out for kNoIndex.
void write_slot(Json& doc, Json& material, Slot slot, int image) {
  const SlotPlace place = place_of(slot);
  Json& holder = writable_holder(material, place);
  if (image == kNoIndex) {
    holder.erase(place.key);
  } else {
    point_slot(holder, place.key, texture_showing(doc, image));
  }
}

void write_slots(Json& doc, Json& material, const MaterialDesc& desc) {
  for (const Slot slot : kSlots) {
    const int image = image_in(desc, slot);
    if (slot_image(doc, material, slot) != image) write_slot(doc, material, slot, image);
  }
}

}  // namespace redgem::material_textures
