#pragma once

// What the editor's Agent panel hands an agent: `redgem mcp <game> --editor <connection>` as its
// MCP server, with the editor's local bridge enabled the first time a session asks for it.

#include <memory>

#include "editor/editor_options.hpp"
#include "project/editor_bridge.hpp"

namespace redgem {

// The hooks for one editor; `bridge` (the editor's, when --editor-bridge already enabled it) must
// outlive the editor's run, and is enabled into when still empty.
AgentHooks agent_hooks(std::unique_ptr<tools::EditorBridge>& bridge);

}  // namespace redgem
