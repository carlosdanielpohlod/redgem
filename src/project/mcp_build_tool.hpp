#pragma once

// MCP's `build` tool: a standalone game made from the saved files, on a thread of its own, as the
// editor's Build menu makes it.
//
//   start    builds for `target` (this system by default) into a folder next to the game
//            ("<game>-build", or "<game>-build-<target>"; `folder` names another one there)
//   status   how the build goes (its step and time), or how it ended: what it made, or why not.
//            `wait` waits up to that many seconds (25 at most) for it to end first.
//
// One build at a time, in this server's process. What it logged reaches `events` once status
// takes how it ended.

#include "project/mcp_tools.hpp"

namespace redgem::tools::mcp {

Json build_tool(const Options& options, const Json& args);
// Waits for a build that still runs: the server calls it before it ends.
void wait_for_builds();

}  // namespace redgem::tools::mcp
