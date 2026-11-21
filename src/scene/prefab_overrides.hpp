#pragma once

// What one placement of a prefab file changes in the parts it inherits from that file. The
// placement keeps these in its `prefab_overrides` property, a JSON object keyed by part path:
//
//   {"lid": {"position": [0, 1, 0], "props": {"tag": "open"}, "unset": ["mass"]},
//    "wheel/hub": {"removed": true}}
//
// A path names a part by the identities of the source nodes from the placement down to it,
// joined by '/' (a nested prefab's parts add its own identities). Everything not listed comes
// from the file, so editing the file changes every placement except what it overrides.

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem::prefabs {

inline constexpr char kPathSeparator = '/';

// Transform fields replace that field of the part's local transform as resolved: for a nested
// placement, after its own file's root offset was combined in.
// The fields of a part an override can hold, as the file and the tools name them.
namespace field {
inline constexpr const char* kPosition = "position";
inline constexpr const char* kRotation = "rotation";
inline constexpr const char* kScale = "scale";
inline constexpr const char* kProps = "props";
inline constexpr const char* kUnset = "unset";
inline constexpr const char* kLight = "light";
inline constexpr const char* kCamera = "camera";
inline constexpr const char* kRemoved = "removed";
// "props.<key>" names one property (set or unset) when reverting a single field.
inline constexpr const char* kPropPrefix = "props.";
}  // namespace field

// The overridden fields of one inherited part. Unset optionals and empty maps keep the file's value.
struct PartOverride {
  std::optional<Vec3> position;
  std::optional<Quat> rotation;
  std::optional<Vec3> scale;
  PropertyMap props;            // set or replaced
  std::set<std::string> unset;  // properties the file gives and this placement drops
  std::optional<LightDesc> light;
  std::optional<CameraDesc> camera;
  bool removed = false;  // the part and everything under it are left out

  bool empty() const;
  bool operator==(const PartOverride& other) const;
};

// By part path.
using Overrides = std::map<std::string, PartOverride>;

// Reads a placement's overrides. Throws std::runtime_error, naming the problem, on malformed text.
Overrides read_overrides(const PropertyMap& placement);
// Writes them back: an empty set removes the property.
void write_overrides(PropertyMap& placement, const Overrides& overrides);
std::string overrides_text(const Overrides& overrides);
Overrides parse_overrides(const std::string& text);

// The part with its overrides applied.
void apply_override(NodeDesc& part, const PartOverride& change);

// The names of a part override's fields, as revert_field takes them ("position", "props.tag").
std::vector<std::string> overridden_fields(const PartOverride& change);
// Drops one field (see overridden_fields) of a part's override, the whole part when `name` is
// empty. Parts left with nothing are removed. False when there was nothing to drop.
bool revert_field(Overrides& overrides, const std::string& path, const std::string& name);

// The path of a part under its parent's path.
std::string child_path(const std::string& parent, const std::string& identity);

}  // namespace redgem::prefabs
