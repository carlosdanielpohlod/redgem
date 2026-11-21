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

// The inherited part at `path` of a placement, as the scene resolves it (`own`: with the
// placement's override of that part; else as the file gives it); nullopt with `error`.
std::optional<NodeDesc> inherited_part(const SceneDocument& document, int placement,
                                       const std::filesystem::path& game_dir, const std::string& path, bool own,
                                       std::string& error);

// The whole override that `words`, applied to the part as it is (`current`), make of it against
// the file's part (`base`): a field back at the file's value is no longer overridden. nullopt with
// `error` on a word that is wrong or names what a part cannot override (name, parent). `notes`
// gets what is worth saying (a script not there yet).
std::optional<prefabs::PartOverride> part_change(const NodeDesc& current, const NodeDesc& base,
                                                 const std::vector<std::string>& words,
                                                 const std::filesystem::path& scripts_dir, std::string& error,
                                                 std::vector<std::string>& notes);

// One line per overridden field: "<path> <field>=<value>", the root as ".".
std::string overrides_listing(const std::vector<prefab_instances::Field>& fields);

}  // namespace redgem::tools
