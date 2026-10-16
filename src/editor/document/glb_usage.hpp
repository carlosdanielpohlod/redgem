#pragma once

// Which entries of a .glb's lists its objects reach: what the nodes draw with, light and see
// with, what moves them, and what all of that is made of.

#include <array>
#include <cstddef>
#include <exception>
#include <functional>
#include <vector>

#include <nlohmann/json.hpp>

#include "editor/document/gltf_json.hpp"

namespace redgem::glb_usage {

// The lists of a file whose entries can go unused.
enum Kind {
  kMeshKind,
  kMaterialKind,
  kTextureKind,
  kImageKind,
  kSamplerKind,
  kAccessorKind,
  kBufferViewKind,
  kCameraKind,
  kLightKind,
  kKinds
};
inline constexpr std::array<const char*, kKinds> kListNames = {
    gltf_json::kMeshes,    gltf_json::kMaterials,   gltf_json::kTextures, gltf_json::kImages, gltf_json::kSamplers,
    gltf_json::kAccessors, gltf_json::kBufferViews, gltf_json::kCameras,  gltf_json::kLights};

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

// What adds lights to a file or to one of its nodes (its "KHR_lights_punctual" extension);
// nullptr when it has none.
nlohmann::json* lights_extension(nlohmann::json& object);
// What the list of `kind` is kept in: the file itself, or for lights the extension that adds them
// (nullptr when the file has none).
nlohmann::json* holder_of(nlohmann::json& doc, Kind kind);

// Refuses (NotFollowed) an object with extensions other than `known`: they may point at the
// file's lists in ways of their own.
void refuse_other_extensions(const nlohmann::json& object, const char* known);

// The entry a reference points at; refused when it points at none.
std::size_t entry_of(const nlohmann::json& reference, std::size_t entries);

// Every reference of the file that starts from something used, in an order that reaches each
// list after everything that points into it: the objects, then what they are made of.
void walk(nlohmann::json& doc, const Usage& usage, const Use& use);

// Which entries of each list the file's nodes reach, through everything in between.
Usage usage_of(nlohmann::json& doc);

}  // namespace redgem::glb_usage
