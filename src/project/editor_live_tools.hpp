#pragma once

#include "editor/editor_session.hpp"
#include "project/mcp_protocol.hpp"

namespace redgem::tools {
// What a tool about the running game answers while the editor edits instead.
inline constexpr const char* kEditorNotPlaying =
    "the editor is not playing; start the game with editor action=play first";
// The game the editor plays, or null while it edits.
Engine* playing_runtime(EditorSession& session);

// Reads unsaved state; set is atomic and leaves the scene dirty rather than saving it.
mcp::Json invoke_editor(EditorSession& session, const std::string& name, const mcp::Json& args, bool busy = false,
                        const mcp::Invoke& window = {});
}  // namespace redgem::tools
