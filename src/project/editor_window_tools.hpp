#pragma once

#include "project/mcp_protocol.hpp"

namespace redgem {
class EditorApp;
namespace tools {
mcp::Json editor_window_tool(EditorApp& app, const std::string& action, const mcp::Json& args);
}
}  // namespace redgem
