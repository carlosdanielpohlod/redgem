#include "editor/document/material_textures.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>

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

// The picture a texture shows through one of its extensions, when it has that one.
std::optional<int> extension_source(const Json& texture, const char* extension) {
  if (!has_object(texture, kExtensions)) return std::nullopt;
  const auto found = texture[kExtensions].find(extension);
  return found == texture[kExtensions].end() ? std::nullopt : source_of(*found);
}

// The picture a texture shows, as the engine picks it: its KTX2 one (KHR_texture_basisu), its
// WebP one (EXT_texture_webp), or its plain one.
std::optional<int> shown_source(const Json& texture) {
  if (const auto ktx2 = extension_source(texture, kBasisuTextureExtension)) return ktx2;
  if (const auto webp = extension_source(texture, kWebpTextureExtension)) return webp;
  return source_of(texture);
}

// Whether a texture says it shows `image` in any of the ways it may: its KTX2, WebP or plain
// picture. (Which one the engine shows depends on whether it reads the KTX2 one, which the
// document alone does not tell; a texture is the same picture whichever it shows.)
bool has_source(const Json& texture, int image) {
  const std::optional<int> sources[] = {extension_source(texture, kBasisuTextureExtension),
                                        extension_source(texture, kWebpTextureExtension), source_of(texture)};
  return std::find(std::begin(sources), std::end(sources), std::optional<int>{image}) != std::end(sources);
}

// The texture extension a picture of this kind is shown through.
struct ExtensionPicture {
  const char* mime_type;
  const char* extension;
};
constexpr ExtensionPicture kExtensionPictures[] = {{kWebpMime, kWebpTextureExtension},
                                                   {kKtx2Mime, kBasisuTextureExtension}};

// The extension a texture shows `image` through; nullptr for a PNG or JPEG one (its plain source).
const char* texture_extension_of(const Json& doc, int image) {
  const Json* entry = entry_at(doc, kImages, image);
  if (entry == nullptr) return nullptr;
  const std::string mime_type = text_at(*entry, kMimeType);
  for (const ExtensionPicture& kind : kExtensionPictures) {
    if (mime_type == kind.mime_type) return kind.extension;
  }
  return nullptr;
}

Json texture_for(const char* extension, int image) {
  if (extension == nullptr) return {{kSource, image}};
  return {{kExtensions, {{extension, {{kSource, image}}}}}};
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
  return has_source(doc[kTextures][texture], image) && plain_sampling(doc, texture);
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
  return shown_source(*entry).value_or(kNoIndex);
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
  // (Without a picture another reader can show, a WebP or KTX2 one is required to read the file.)
  const char* extension = texture_extension_of(doc, image);
  if (extension != nullptr) add_extension_required(doc, extension);
  array_at(doc, kTextures).push_back(texture_for(extension, image));
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
