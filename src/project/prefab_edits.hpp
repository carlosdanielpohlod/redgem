#pragma once

// MCP's scene_prefab tool: what a scene does with one of its prefab placements
// (docs/prefabs.md#overrides), the same on the saved scene and in the editor's open one.
//
//   overrides   the fields the placement overrides, a line each ("lid position=0,1,0"; "." is the
//               placement itself)
//   override    sets fields of one inherited `part` with `assignments` (the words of scene_edit's
//               set, but name and parent; prop.key= drops a property the file gives; removed=true)
//   revert      drops one `field` of a `part`, every override of a part, or all of them
//   apply       writes the overrides and added children into the prefab file (confirm=true)
//   unpack      replaces the placement with plain objects from its file (confirm=true)
//
// Each change is one undo step; nothing changes when one fails (it throws what to change).

#include <filesystem>
#include <string>

#include "editor/document/scene_document.hpp"
#include "project/mcp_protocol.hpp"

namespace redgem::tools {

struct PrefabOutcome {
  std::string text;
  int object = kNoIndex;  // the placement, or the root unpack made
};

PrefabOutcome edit_prefab(SceneDocument& document, const std::filesystem::path& game_dir, const mcp::Json& args);

// Whether an action only reads (what --read-only keeps).
bool reads_prefab(const std::string& action);

}  // namespace redgem::tools
