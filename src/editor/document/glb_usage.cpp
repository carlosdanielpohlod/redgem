#include "editor/document/glb_usage.hpp"

#include <string_view>

namespace redgem::glb_usage {
namespace {

using namespace gltf_json;
using Json = nlohmann::json;

constexpr std::string_view kTextureSuffix = "Texture";  // "baseColorTexture", "normalTexture"...
// How deep a material's extensions are walked for texture references (they nest a few levels).
constexpr int kMaxMaterialDepth = 64;

bool ends_with(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// A value of a material that may hold texture references, not too deep to be looked into.
bool walkable(const Json& value, int depth) { return value.is_object() && depth <= kMaxMaterialDepth; }

// The entries of one of the file's lists that are used.
template <typename Visit>
void each_used(Json& doc, Kind kind, const Usage& usage, Visit visit) {
  std::size_t index = 0;
  each(doc, kListNames[kind], [&](Json& entry) {
    if (usage[kind][index++]) visit(entry);
  });
}

void use_if_there(Json& object, const char* key, Kind kind, const Use& use) {
  if (object.contains(key)) use(object[key], kind);
}

// The same for every reference of a set of named ones ("POSITION": 3, "NORMAL": 4), when the
// object holds such a set under `key`.
void use_each_named(Json& object, const char* key, Kind kind, const Use& use) {
  const auto named = object.find(key);
  if (named == object.end()) return;
  for (auto& [name, reference] : named->items()) use(reference, kind);
}

// An extension may point at the file's lists in ways of its own: only `known` is looked past.
void refuse_other_extensions(const Json& object, const char* known) {
  const auto extensions = object.find(kExtensions);
  if (extensions == object.end()) return;
  for (const auto& [name, extension] : extensions->items()) {
    if (name != known) throw NotFollowed();
  }
}

void walk_primitive(Json& primitive, const Use& use) {
  if (primitive.contains(kExtensions)) throw NotFollowed();
  use_each_named(primitive, kAttributes, kAccessorKind, use);
  use_if_there(primitive, kIndices, kAccessorKind, use);
  use_if_there(primitive, kMaterial, kMaterialKind, use);
  each(primitive, kTargets, [&use](Json& target) {
    for (auto& [name, accessor] : target.items()) use(accessor, kAccessorKind);
  });
}

// Every texture a material draws with: {"index": n} under a key ending in "Texture", also inside
// its extensions.
void walk_material(Json& value, const Use& use, int depth) {
  if (!walkable(value, depth)) return;
  for (auto& [key, child] : value.items()) {
    if (ends_with(key, kTextureSuffix)) use_if_there(child, kIndex, kTextureKind, use);
    walk_material(child, use, depth + 1);
  }
}

void walk_texture(Json& texture, const Use& use) {
  use_if_there(texture, kSampler, kSamplerKind, use);
  use_if_there(texture, kSource, kImageKind, use);
  const auto extensions = texture.find(kExtensions);
  if (extensions == texture.end()) return;
  for (auto& [name, extension] : extensions->items()) use_if_there(extension, kSource, kImageKind, use);
}

void walk_accessor(Json& accessor, const Use& use) {
  if (accessor.contains(kExtensions)) throw NotFollowed();
  use_if_there(accessor, kBufferView, kBufferViewKind, use);
  const auto sparse = accessor.find(kSparse);
  if (sparse == accessor.end()) return;
  for (const char* part : {kIndices, kValues}) {
    if (sparse->contains(part)) use_if_there((*sparse)[part], kBufferView, kBufferViewKind, use);
  }
}

// What moves the objects: every skeleton and animation the file has is used.
void walk_motion(Json& doc, const Use& use) {
  each(doc, kSkins, [&use](Json& skin) { use_if_there(skin, kInverseBindMatrices, kAccessorKind, use); });
  each(doc, kAnimations, [&use](Json& animation) {
    each(animation, kSamplers, [&use](Json& sampler) {
      use_if_there(sampler, kInput, kAccessorKind, use);
      use_if_there(sampler, kOutput, kAccessorKind, use);
    });
  });
}

// What is drawn with: materials, their textures and the pictures of those.
void walk_looks(Json& doc, const Usage& usage, const Use& use) {
  each_used(doc, kMaterialKind, usage, [&use](Json& material) { walk_material(material, use, 0); });
  each_used(doc, kTextureKind, usage, [&use](Json& texture) { walk_texture(texture, use); });
  each_used(doc, kImageKind, usage, [&use](Json& image) { use_if_there(image, kBufferView, kBufferViewKind, use); });
}

// What a node is drawn with, and the camera and light it carries.
void walk_node(Json& node, const Use& use) {
  refuse_other_extensions(node, kLightsExtension);
  use_if_there(node, kMesh, kMeshKind, use);
  use_if_there(node, kCamera, kCameraKind, use);
  if (Json* lights = lights_extension(node)) use_if_there(*lights, kLight, kLightKind, use);
}

std::size_t entries_of(Json& doc, Kind kind) {
  const Json* holder = holder_of(doc, kind);
  if (holder == nullptr) return 0;
  const auto list = holder->find(kListNames[kind]);
  return list != holder->end() ? list->size() : 0;
}

// Nothing used yet, with a place for every entry of every list.
Usage nothing_used(Json& doc) {
  Usage usage;
  for (std::size_t kind = 0; kind < kKinds; ++kind) usage[kind].assign(entries_of(doc, static_cast<Kind>(kind)), false);
  return usage;
}

}  // namespace

Json* lights_extension(Json& object) {
  const auto extensions = object.find(kExtensions);
  if (extensions == object.end()) return nullptr;
  const auto lights = extensions->find(kLightsExtension);
  return lights != extensions->end() ? &*lights : nullptr;
}

Json* holder_of(Json& doc, Kind kind) { return kind == kLightKind ? lights_extension(doc) : &doc; }

std::size_t entry_of(const Json& reference, std::size_t entries) {
  const int index = index_from(reference).value_or(kNoIndex);
  if (!in_range(index, entries)) throw NotFollowed();
  return static_cast<std::size_t>(index);
}

void walk(Json& doc, const Usage& usage, const Use& use) {
  refuse_other_extensions(doc, kLightsExtension);
  each(doc, kNodes, [&use](Json& node) { walk_node(node, use); });
  each_used(doc, kMeshKind, usage, [&use](Json& mesh) {
    each(mesh, kPrimitives, [&use](Json& primitive) { walk_primitive(primitive, use); });
  });
  walk_motion(doc, use);
  walk_looks(doc, usage, use);
  each_used(doc, kAccessorKind, usage, [&use](Json& accessor) { walk_accessor(accessor, use); });
}

Usage usage_of(Json& doc) {
  Usage usage = nothing_used(doc);
  walk(doc, usage,
       [&usage](Json& reference, Kind kind) { usage[kind][entry_of(reference, usage[kind].size())] = true; });
  return usage;
}

}  // namespace redgem::glb_usage
