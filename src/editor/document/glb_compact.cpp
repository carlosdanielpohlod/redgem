#include "editor/document/glb_compact.hpp"

#include <algorithm>
#include <array>
#include <exception>

#include "editor/document/glb_usage.hpp"
#include "editor/document/gltf_json.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {
namespace {

using namespace gltf_json;
using namespace glb_usage;
using Json = nlohmann::json;

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
  Json* holder = holder_of(doc, kind);
  if (holder == nullptr) return;
  Json kept = Json::array();
  std::size_t index = 0;
  each(*holder, kListNames[kind], [&](Json& entry) {
    if (used[index++]) kept.push_back(std::move(entry));
  });
  set_unless_empty(*holder, kListNames[kind], kept);
}

// Whether the lights extension is there and adds no light any more (glTF wants at least one in it).
bool emptied(const Json* lights) { return lights != nullptr && lights->empty(); }

// Takes the lights extension out of a file left with no light: out of its extensions, and out of
// the ones it says it uses.
void drop_emptied_lights(Json& doc) {
  if (!emptied(lights_extension(doc))) return;
  doc[kExtensions].erase(kLightsExtension);
  if (doc[kExtensions].empty()) doc.erase(kExtensions);
  remove_extension_used(doc, kLightsExtension);
}

// A buffer view kept the plain way: no extension, in the file's only buffer.
bool plain_view(const Json& view) {
  return !view.contains(kExtensions) && view.value(kBuffer, kOnlyBuffer) == kOnlyBuffer;
}

bool inside(std::size_t offset, std::size_t length, std::size_t whole) {
  return offset <= whole && length <= whole - offset;
}

// Where a buffer view's bytes are in a binary chunk of `bin_size` bytes: first and one past last.
std::pair<std::size_t, std::size_t> bytes_of(const Json& view, std::size_t bin_size) {
  if (!plain_view(view)) throw NotFollowed();
  const auto offset = view.value(kByteOffset, std::size_t{0});
  const auto length = view.at(kByteLength).get<std::size_t>();
  if (!inside(offset, length, bin_size)) throw NotFollowed();
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

std::optional<GlbFile> lean_copy(const Json& json, const std::vector<std::uint8_t>& bin) {
  GlbFile lean;
  lean.json = json;
  const Usage usage = usage_of(lean.json);
  if (all_used(usage)) return std::nullopt;
  renumber(lean.json, usage);
  for (std::size_t kind = 0; kind < kKinds; ++kind) keep_used(lean.json, static_cast<Kind>(kind), usage[kind]);
  drop_emptied_lights(lean.json);
  lean.bin = packed_bytes(lean.json, bin);
  describe_buffer(lean.json, lean.bin.size());
  return lean;
}

// The file as if its objects were one for each mesh still held, with every node's extensions,
// skeleton and animation the editor keeps: what is reached from those is still used.
Json still_held(const Json& doc, const std::set<int>& released, const GlbSources& sources) {
  for (const Json& node : sources.nodes) refuse_other_extensions(node, kLightsExtension);
  Json probe = doc;
  Json nodes = Json::array();
  const std::size_t meshes = probe.contains(kMeshes) ? probe[kMeshes].size() : 0;
  for (std::size_t mesh = 0; mesh < meshes; ++mesh) {
    if (released.count(static_cast<int>(mesh)) == 0) nodes.push_back({{kMesh, mesh}});
  }
  probe[kNodes] = std::move(nodes);
  probe[kSkins] = sources.skins;
  probe[kAnimations] = sources.animations;
  return probe;
}

// How long the longest view out of use is (each view is checked to be where the chunk has bytes).
std::size_t longest_unused(const Json& views, const std::vector<bool>& used, std::size_t bin_size) {
  std::size_t longest = 0;
  for (std::size_t view = 0; view < views.size(); ++view) {
    const auto [first, last] = bytes_of(views[view], bin_size);
    if (!used[view]) longest = std::max(longest, last - first);
  }
  return longest;
}

Json views_in_use(const Json& views, const std::vector<bool>& used) {
  Json in_use = Json::array();
  for (std::size_t view = 0; view < views.size(); ++view) {
    if (used[view]) in_use.push_back(views[view]);
  }
  return in_use;
}

// The views in use packed one after the other; the others keep their length and point at the
// chunk's start, which is made long enough for them.
std::vector<std::uint8_t> bytes_in_use(Json& views, const std::vector<bool>& used,
                                       const std::vector<std::uint8_t>& bin) {
  const std::size_t unused_length = longest_unused(views, used, bin.size());
  Json in_use = Json::object();
  in_use[kBufferViews] = views_in_use(views, used);
  std::vector<std::uint8_t> packed = packed_bytes(in_use, bin);
  packed.resize(std::max(packed.size(), unused_length));
  std::size_t next = 0;
  for (std::size_t view = 0; view < views.size(); ++view) {
    views[view][kByteOffset] = used[view] ? in_use[kBufferViews][next++][kByteOffset] : Json(0);
  }
  return packed;
}

// The images `held` (and the bytes they are made of) count as used.
void hold_images(const Json& doc, const std::vector<bool>& held, Usage& usage) {
  const std::size_t images = std::min(held.size(), usage[kImageKind].size());
  for (std::size_t image = 0; image < images; ++image) {
    if (!held[image]) continue;
    usage[kImageKind][image] = true;
    const Json& entry = doc[kImages][image];
    if (entry.contains(kBufferView))
      usage[kBufferViewKind][entry_of(entry[kBufferView], usage[kBufferViewKind].size())] = true;
  }
}

std::optional<std::vector<bool>> release_in_place(GlbFile& glb, const std::set<int>& released,
                                                  const GlbSources& sources, const std::vector<bool>& held_images) {
  Json probe = still_held(glb.json, released, sources);
  Usage usage = usage_of(probe);
  hold_images(glb.json, held_images, usage);
  Json views = list_of(glb.json, kBufferViews);
  std::vector<std::uint8_t> bin = bytes_in_use(views, usage[kBufferViewKind], glb.bin);
  set_unless_empty(glb.json, kBufferViews, views);
  glb.bin = std::move(bin);
  return usage[kImageKind];
}

}  // namespace

std::optional<std::vector<bool>> release_meshes(GlbFile& glb, const std::set<int>& released, const GlbSources& sources,
                                                const std::vector<bool>& held_images) {
  try {
    return release_in_place(glb, released, sources, held_images);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<GlbFile> without_unused(const GlbFile& glb) {
  try {
    return lean_copy(glb.json, glb.bin);
  } catch (const std::exception&) {
    // (Also what the JSON library throws when the file holds something else than glTF says.)
    return std::nullopt;
  }
}

std::optional<GlbFile> without_unused(const nlohmann::json& json, const std::vector<std::uint8_t>& bin) {
  try {
    return lean_copy(json, bin);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

}  // namespace redgem
