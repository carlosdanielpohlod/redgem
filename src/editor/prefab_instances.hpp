#pragma once

// What a scene does with one of its prefab placements (docs/prefabs.md#overrides): read and
// change its overrides, revert them, apply them to the prefab file, or unpack the placement into
// plain objects. Each change of the scene is one undo step; nothing changes when one fails.

#include <filesystem>
#include <string>
#include <vector>

#include "editor/document/scene_document.hpp"
#include "scene/prefab_overrides.hpp"

namespace redgem::prefab_instances {

// Whether a node is a placement of a prefab file (it has props::kPrefabFile).
bool is_placement(const SceneDocument& document, int node);

// One overridden field, as `redgem scene overrides` lists it. The placement's own properties are
// fields of the part "" (its root).
struct Field {
  std::string path;
  std::string name;   // see prefabs::overridden_fields: "position", "props.tag"...
  std::string value;  // short text: "0,1,0", "open", "unset", "true"
};
std::vector<Field> fields(const SceneDocument& document, int placement);

// The overrides a placement holds (empty when none, or not a placement).
prefabs::Overrides overrides_of(const SceneDocument& document, int placement);

// Adds `change` to the part at `path` (prefabs::merge_override). One undo step.
bool set_override(SceneDocument& document, int placement, const std::string& path, const prefabs::PartOverride& change,
                  std::string& error);

// Makes `change` the whole override of the part at `path` (an empty one removes it). One undo step.
bool replace_override(SceneDocument& document, int placement, const std::string& path,
                      const prefabs::PartOverride& change, std::string& error);

// Drops one field of a part (`path` "" with "props.<key>": a root property the placement
// overrides); a whole part when `name` is empty; every override and root property when both are.
// Children added under the placement are kept. One undo step.
bool revert(SceneDocument& document, int placement, const std::string& path, const std::string& name,
            std::string& error);

// Writes the placement's overrides, root properties and added children into its prefab file (and
// saves it), then takes them off the placement: every placement of the file gets them. A part of
// a nested prefab becomes an override of that nested placement in the file. The scene change is
// one undo step; the file stays written.
bool apply(SceneDocument& document, int placement, const std::filesystem::path& game_dir, std::string& error);

// Replaces the placement with plain objects copied from its prefab file, its overrides applied and
// its added children kept. Nested placements in the file stay placements (one level). Returns the
// new root, or kNoIndex with `error`. One undo step.
int unpack(SceneDocument& document, int placement, const std::filesystem::path& game_dir, std::string& error);

}  // namespace redgem::prefab_instances
