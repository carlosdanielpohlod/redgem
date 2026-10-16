#pragma once

// Which entries of a .glb's lists its objects reach: what the nodes draw with, what moves them,
// and what all of that is made of.

#include <array>
#include <cstddef>
#include <exception>
#include <functional>
#include <vector>

#include <nlohmann/json.hpp>

#include "editor/document/gltf_json.hpp"

namespace redgem::glb_usage {

// The lists of a file whose entries can go unused.
enum Kind { kMeshKind, kMaterialKind, kTextureKind, kImageKind, kSamplerKind, kAccessorKind, kBufferViewKind, kKinds };
inline constexpr std::array<const char*, kKinds> kListNames = {
    gltf_json::kMeshes,   gltf_json::kMaterials, gltf_json::kTextures,   gltf_json::kImages,
    gltf_json::kSamplers, gltf_json::kAccessors, gltf_json::kBufferViews};

// The file points at its data in a way that is not followed here: it is left as it is.
struct NotFollowed : std::exception {};

// Which entries of each list something points at.
using Usage = std::array<std::vector<bool>, kKinds>;
// What is done with each reference found: `reference` is an index into the list of `kind`.
using Use = std::function<void(nlohmann::json& reference, Kind kind)>;

// Calls `visit` with each entry of the list `object` holds under `key`, when it holds one.
template <typename Visit>
void each(nlohmann::json& object, const char* key, Visit visit) {
  const auto list = object.find(key);
  if (list == object.end()) return;
  if (!list->is_array()) throw NotFollowed();
  for (nlohmann::json& entry : *list) visit(entry);
}

// The entry a reference points at; refused when it points at none.
std::size_t entry_of(const nlohmann::json& reference, std::size_t entries);

// Every reference of the file that starts from something used, in an order that reaches each
// list after everything that points into it: the objects, then what they are made of.
void walk(nlohmann::json& doc, const Usage& usage, const Use& use);

// Which entries of each list the file's nodes reach, through everything in between.
Usage usage_of(nlohmann::json& doc);

}  // namespace redgem::glb_usage
