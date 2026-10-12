#include "editor/glb_compact.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <functional>
#include <string_view>

#include "editor/gltf_json.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {
namespace {

using namespace gltf_json;
using Json = nlohmann::json;

// The lists of a file whose entries can go unused.
enum Kind { kMeshKind, kMaterialKind, kTextureKind, kImageKind, kSamplerKind, kAccessorKind, kBufferViewKind, kKinds };
constexpr std::array<const char*, kKinds> kListNames = {kMeshes,   kMaterials, kTextures,   kImages,
                                                        kSamplers, kAccessors, kBufferViews};
constexpr std::string_view kTextureSuffix = "Texture";  // "baseColorTexture", "normalTexture"...
// How deep a material's extensions are walked for texture references (they nest a few levels).
constexpr int kMaxMaterialDepth = 64;

// The file points at its data in a way that is not followed here: it is left as it is.
struct NotFollowed : std::exception {};

// Which entries of each list something points at.
using Usage = std::array<std::vector<bool>, kKinds>;
// What is done with each reference found: `reference` is an index into the list of `kind`.
using Use = std::function<void(Json& reference, Kind kind)>;

bool ends_with(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Calls `visit` with each entry of the list `object` holds under `key`, when it holds one.
template <typename Visit>
void each(Json& object, const char* key, Visit visit) {
  const auto list = object.find(key);
  if (list == object.end()) return;
  if (!list->is_array()) throw NotFollowed();
  for (Json& entry : *list) visit(entry);
}

// The same, for the entries of one of the file's lists that are used.
template <typename Visit>
void each_used(Json& doc, Kind kind, const Usage& usage, Visit visit) {
  std::size_t index = 0;
  each(doc, kListNames[kind], [&](Json& entry) {
    if (usage[kind][index++]) visit(entry);
  });
}

void use_if_there(Json& object, const char* key, Kind kind, const Use& use) {
  if (object.is_object() && object.contains(key)) use(object[key], kind);
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
  if (!value.is_object() || depth > kMaxMaterialDepth) return;
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

// Every reference of the file that starts from something used, in an order that reaches each
// list after everything that points into it: the objects, then what they are made of.
void walk(Json& doc, const Usage& usage, const Use& use) {
  refuse_other_extensions(doc, kLightsExtension);
  each(doc, kNodes, [&use](Json& node) {
    refuse_other_extensions(node, kLightsExtension);
    use_if_there(node, kMesh, kMeshKind, use);
  });
  each_used(doc, kMeshKind, usage, [&use](Json& mesh) {
    each(mesh, kPrimitives, [&use](Json& primitive) { walk_primitive(primitive, use); });
  });
  walk_motion(doc, use);
  walk_looks(doc, usage, use);
  each_used(doc, kAccessorKind, usage, [&use](Json& accessor) { walk_accessor(accessor, use); });
}

// The entry a reference points at; refused when it points at none.
std::size_t entry_of(const Json& reference, std::size_t entries) {
  const std::optional<int> index = index_from(reference);
  if (!index || static_cast<std::size_t>(*index) >= entries) throw NotFollowed();
  return static_cast<std::size_t>(*index);
}

// Nothing used yet, with a place for every entry of every list.
Usage nothing_used(const Json& doc) {
  Usage usage;
  for (std::size_t kind = 0; kind < kKinds; ++kind) {
    const auto list = doc.find(kListNames[kind]);
    usage[kind].assign(list != doc.end() ? list->size() : 0, false);
  }
  return usage;
}

Usage usage_of(Json& doc) {
  Usage usage = nothing_used(doc);
  walk(doc, usage,
       [&usage](Json& reference, Kind kind) { usage[kind][entry_of(reference, usage[kind].size())] = true; });
  return usage;
}

bool all_used(const Usage& usage) {
  return std::all_of(usage.begin(), usage.end(), [](const std::vector<bool>& used) {
    return std::all_of(used.begin(), used.end(), [](bool one) { return one; });
  });
}

// The number each used entry gets once the unused ones are gone.
std::vector<int> new_numbers(const std::vector<bool>& used) {
  std::vector<int> numbers(used.size(), kNoIndex);
  int next = 0;
  for (std::size_t i = 0; i < used.size(); ++i) {
    if (used[i]) numbers[i] = next++;
  }
  return numbers;
}

// Points every reference at its entry's new number.
void renumber(Json& doc, const Usage& usage) {
  std::array<std::vector<int>, kKinds> numbers;
  for (std::size_t kind = 0; kind < kKinds; ++kind) numbers[kind] = new_numbers(usage[kind]);
  walk(doc, usage, [&numbers](Json& reference, Kind kind) {
    reference = numbers[kind][entry_of(reference, numbers[kind].size())];
  });
}

// Takes the unused entries out of one of the file's lists (and the list, once it is empty).
void keep_used(Json& doc, Kind kind, const std::vector<bool>& used) {
  if (!doc.contains(kListNames[kind])) return;
  Json kept = Json::array();
  std::size_t index = 0;
  for (Json& entry : doc[kListNames[kind]]) {
    if (used[index++]) kept.push_back(std::move(entry));
  }
  set_unless_empty(doc, kListNames[kind], kept);
}

// Where a buffer view's bytes are in a binary chunk of `bin_size` bytes: first and one past last.
std::pair<std::size_t, std::size_t> bytes_of(const Json& view, std::size_t bin_size) {
  if (view.contains(kExtensions) || view.value(kBuffer, kOnlyBuffer) != kOnlyBuffer) throw NotFollowed();
  const auto offset = view.value(kByteOffset, std::size_t{0});
  const auto length = view.at(kByteLength).get<std::size_t>();
  if (offset > bin_size || length > bin_size - offset) throw NotFollowed();
  return {offset, offset + length};
}

// The bytes the buffer views left in the file point at, one view after the other; each view is
// pointed at where its bytes went.
std::vector<std::uint8_t> packed_bytes(Json& doc, const std::vector<std::uint8_t>& bin) {
  std::vector<std::uint8_t> packed;
  each(doc, kBufferViews, [&](Json& view) {
    const auto [first, last] = bytes_of(view, bin.size());
    packed.resize(glb_padded_size(packed.size()));  // data starts on multiples of 4 bytes
    view[kByteOffset] = packed.size();
    packed.insert(packed.end(), bin.begin() + static_cast<std::ptrdiff_t>(first),
                  bin.begin() + static_cast<std::ptrdiff_t>(last));
  });
  return packed;
}

void describe_buffer(Json& doc, std::size_t bytes) {
  if (bytes == 0) {
    doc.erase(kBuffers);
  } else {
    doc[kBuffers] = Json::array({{{kByteLength, bytes}}});
  }
}

std::optional<GlbFile> lean_copy(const GlbFile& glb) {
  GlbFile lean;
  lean.json = glb.json;
  const Usage usage = usage_of(lean.json);
  if (all_used(usage)) return std::nullopt;
  renumber(lean.json, usage);
  for (std::size_t kind = 0; kind < kKinds; ++kind) keep_used(lean.json, static_cast<Kind>(kind), usage[kind]);
  lean.bin = packed_bytes(lean.json, glb.bin);
  describe_buffer(lean.json, lean.bin.size());
  return lean;
}

}  // namespace

std::optional<GlbFile> without_unused(const GlbFile& glb) {
  try {
    return lean_copy(glb);
  } catch (const std::exception&) {
    // (Also what the JSON library throws when the file holds something else than glTF says.)
    return std::nullopt;
  }
}

}  // namespace redgem
