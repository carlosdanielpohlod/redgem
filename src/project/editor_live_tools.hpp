#pragma once

#include "editor/editor_session.hpp"
#include "project/mcp_protocol.hpp"

namespace redgem::tools {
// Reads unsaved state; set is atomic and leaves the scene dirty rather than saving it.
mcp::Json invoke_editor(EditorSession& session, const std::string& name, const mcp::Json& args, bool busy = false);
}  // namespace redgem::tools
