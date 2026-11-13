#include "editor/document/glb_dedup.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <exception>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "editor/document/glb_geometry.hpp"
#include "editor/document/glb_usage.hpp"
#include "editor/document/gltf_json.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

using namespace gltf_json;
using namespace glb_usage;
using Json = nlohmann::json;

// Each list in turn, every one after the lists its entries point into: an entry's JSON is only
// compared once its own references are merged.
constexpr Kind kMergeOrder[] = {kBufferViewKind, kAccessorKind, kImageKind, kSamplerKind, kTextureKind};
// Mixes the hash of an entry's JSON with the hash of its bytes.
constexpr std::size_t kHashMultiplier = 31;

// What an entry holds, as far as being the same as another one goes: its JSON, and for a buffer
// view the bytes it points at (its JSON without where they are).
struct Content {
  std::string json;
  std::string_view bytes;
  bool operator==(const Content& other) const { return json == other.json && bytes == other.bytes; }
};

struct ContentHash {
  std::size_t operator()(const Content& content) const {
    return std::hash<std::string>{}(content.json) * kHashMultiplier + std::hash<std::string_view>{}(content.bytes);
  }
};

std::string_view bytes_between(const std::vector<std::uint8_t>& bin, std::size_t first, std::size_t last) {
  static_assert(sizeof(std::uint8_t) == sizeof(char), "bytes are read as characters");
  const auto* start = bin.data() + first;
  return {reinterpret_cast<const char*>(start), last - first};
}

Content content_of(const Json& entry, Kind kind, const std::vector<std::uint8_t>& bin) {
  if (kind != kBufferViewKind) return {entry.dump(), {}};
  const auto [first, last] = bytes_of(entry, bin.size());
  Json without_place = entry;
  without_place.erase(kByteOffset);
  return {without_place.dump(), bytes_between(bin, first, last)};
}

// For each entry of the list of `kind`: the first entry that holds the same (itself when none
// before it does). `own` entries are left alone, and no other entry is merged into them.
std::vector<int> firsts_of(Json& doc, Kind kind, const std::vector<bool>& own, const std::vector<std::uint8_t>& bin) {
  std::unordered_map<Content, int, ContentHash> first_with;
  std::vector<int> firsts;
  each(doc, kListNames[kind], [&](Json& entry) {
    const std::size_t index = firsts.size();
    const int self = static_cast<int>(index);
    firsts.push_back(own[index] ? self : first_with.emplace(content_of(entry, kind, bin), self).first->second);
  });
  return firsts;
}

bool repeats(const std::vector<int>& firsts) {
  for (std::size_t entry = 0; entry < firsts.size(); ++entry) {
    if (firsts[entry] != static_cast<int>(entry)) return true;
  }
  return false;
}

// Every entry of every list counted as used, so that every reference is visited.
Usage everything(Json& doc) {
  Usage all = usage_of_meshes(doc, {});
  for (std::vector<bool>& used : all) used.assign(used.size(), true);
  return all;
}

void point_at_firsts(Json& doc, Kind kind, const std::vector<int>& firsts, const Usage& all) {
  walk(doc, all, [&](Json& reference, Kind referenced) {
    if (referenced == kind) reference = firsts[entry_of(reference, firsts.size())];
  });
}

// What the editor reads as a flag that is set (see get_bool): true, or a number other than zero.
bool is_set(const Json& value) {
  if (value.is_boolean()) return value.get<bool>();
  return value.is_number() && value.get<double>() != 0.0;
}

// A node whose custom properties say it is a terrain.
bool says_terrain(const Json& node) {
  if (!has_object(node, kExtras)) return false;
  const Json& extras = node[kExtras];
  const auto flag = extras.find(props::kTerrain);
  return flag != extras.end() && is_set(*flag);
}

bool terrain_with_mesh(const Json& node) { return says_terrain(node) && node.contains(kMesh); }

std::vector<bool> terrain_meshes(Json& doc, std::size_t meshes) {
  std::vector<bool> terrains(meshes, false);
  each(doc, kNodes, [&](Json& node) {
    if (terrain_with_mesh(node)) terrains[entry_of(node[kMesh], meshes)] = true;
  });
  return terrains;
}

Usage nothing_of(const Usage& all) {
  Usage none;
  for (std::size_t kind = 0; kind < kKinds; ++kind) none[kind].assign(all[kind].size(), false);
  return none;
}

// The entries a terrain's vertices are made of, which stay its own: the accessors of its mesh and
// their buffer views (not the pictures of its material, which are merged like any other).
Usage terrain_entries(Json& doc, const Usage& all) {
  Usage vertices = nothing_of(all);
  vertices[kMeshKind] = terrain_meshes(doc, all[kMeshKind].size());
  vertices[kAccessorKind] = usage_of_meshes(doc, vertices[kMeshKind])[kAccessorKind];
  Usage own = nothing_of(all);
  own[kAccessorKind] = vertices[kAccessorKind];
  walk_from_meshes(doc, vertices, [&own](Json& reference, Kind kind) {
    if (kind == kBufferViewKind) own[kind][entry_of(reference, own[kind].size())] = true;
  });
  return own;
}

// Merges the repeats of each list in turn; whether there were any.
bool merge_all(Json& doc, const std::vector<std::uint8_t>& bin) {
  const Usage all = everything(doc);
  const Usage own = terrain_entries(doc, all);
  bool merged = false;
  for (const Kind kind : kMergeOrder) {
    const std::vector<int> firsts = firsts_of(doc, kind, own[kind], bin);
    if (!repeats(firsts)) continue;
    point_at_firsts(doc, kind, firsts, all);
    merged = true;
  }
  return merged;
}

// How many references point at each entry of each list, from every entry of the file.
using Counts = std::array<std::vector<int>, kKinds>;

Counts reference_counts(Json& doc) {
  const Usage all = everything(doc);
  Counts counts;
  for (std::size_t kind = 0; kind < kKinds; ++kind) counts[kind].assign(all[kind].size(), 0);
  walk(doc, all, [&counts](Json& reference, Kind kind) { ++counts[kind][entry_of(reference, counts[kind].size())]; });
  return counts;
}

bool pointed_at_twice(const Counts& counts, Kind kind, std::size_t entry) { return counts[kind].at(entry) > 1; }

bool view_shared(const Counts& counts, const Json& accessor) {
  const auto view = accessor.find(kBufferView);
  if (view == accessor.end()) return false;
  return pointed_at_twice(counts, kBufferViewKind, entry_of(*view, counts[kBufferViewKind].size()));
}

bool shared(Json& doc, std::size_t accessor) {
  const Counts counts = reference_counts(doc);
  return pointed_at_twice(counts, kAccessorKind, accessor) || view_shared(counts, doc.at(kAccessors).at(accessor));
}

// Appends a copy of the accessor, of its buffer view and of its bytes; returns the accessor's number.
int copy_accessor(GlbFile& glb, std::size_t accessor) {
  Json copy = glb.json.at(kAccessors).at(accessor);
  Json view = glb.json.at(kBufferViews).at(copy.at(kBufferView).get<std::size_t>());
  const auto [first, last] = bytes_of(view, glb.bin.size());
  const std::vector<std::uint8_t> bytes(glb.bin.begin() + static_cast<std::ptrdiff_t>(first),
                                        glb.bin.begin() + static_cast<std::ptrdiff_t>(last));
  view[kByteOffset] = glb_geometry::append_binary(glb, bytes.data(), bytes.size());
  array_at(glb.json, kBufferViews).push_back(std::move(view));
  copy[kBufferView] = last_index(glb.json[kBufferViews]);
  array_at(glb.json, kAccessors).push_back(std::move(copy));
  return last_index(glb.json[kAccessors]);
}

}  // namespace

std::optional<int> unshared_copy(GlbFile& glb, std::size_t accessor) {
  try {
    if (!shared(glb.json, accessor)) return std::nullopt;
    return copy_accessor(glb, accessor);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<Json> with_repeats_merged(const Json& json, const std::vector<std::uint8_t>& bin) {
  try {
    Json doc = json;
    if (!merge_all(doc, bin)) return std::nullopt;
    return doc;
  } catch (const std::exception&) {
    // (Also what the JSON library throws when the file holds something else than glTF says.)
    return std::nullopt;
  }
}

}  // namespace redgem
