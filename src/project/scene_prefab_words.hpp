#pragma once

// The words `redgem scene override` takes for an inherited part of a prefab placement: the same
// as `scene set` (position=, rotation=, scale=, prop.key=, script+=, light_*=, camera_*=), where
// prop.key= drops a property the file gives, plus removed=true. And what `scene overrides` prints.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "editor/document/scene_document.hpp"
#include "editor/prefab_instances.hpp"

namespace redgem::tools {

// The path the command line gives for a placement's root (its own properties).
inline constexpr const char* kPlacementRootWord = ".";

// The inherited part at `path` of a placement, as the scene resolves it; nullopt with `error`.
std::optional<NodeDesc> inherited_part(const SceneDocument& document, int placement,
                                       const std::filesystem::path& game_dir, const std::string& path,
                                       std::string& error);

// What `words` change in `part`; nullopt with `error` on a word that is wrong or changes nothing
// a part can override (name, parent). `notes` gets what is worth saying (a script not there yet).
std::optional<prefabs::PartOverride> part_change(const NodeDesc& part, const std::vector<std::string>& words,
                                                 const std::filesystem::path& scripts_dir, std::string& error,
                                                 std::vector<std::string>& notes);

// One line per overridden field: "<path> <field>=<value>", the root as ".".
std::string overrides_listing(const std::vector<prefab_instances::Field>& fields);

}  // namespace redgem::tools
