#include "project/agent_tools.hpp"

#include "core/platform.hpp"
#include "core/translations.hpp"
#include "editor/editor_app.hpp"
#include "project/editor_bridge_run.hpp"
#include "project/scene_text.hpp"

namespace redgem {
namespace {

constexpr const char* kServerName = "redgem";  // what the agent calls the editor's tools
constexpr const char* kMcpCommand = "mcp";
constexpr const char* kEditorOption = "--editor";

// The editor's own MCP server for the game it edits, through its bridge.
agent::McpServer live_server(EditorApp& editor, const tools::EditorBridge& bridge) {
  return {kServerName,
          platform::executable_path().u8string(),
          {kMcpCommand, editor.session().game_dir().u8string(), kEditorOption, bridge.connection_file().u8string()},
          {}};
}

}  // namespace

AgentHooks agent_hooks(std::unique_ptr<tools::EditorBridge>& bridge) {
  AgentHooks hooks;
  hooks.client_version = REDGEM_VERSION;
  hooks.tool_servers = [&bridge](EditorApp& editor, std::string& error) -> std::vector<agent::McpServer> {
    if (!bridge) bridge = enable_editor_bridge(editor);
    if (bridge) return {live_server(editor, *bridge)};
    error = tr("project.agent.no_bridge");
    return {};
  };
  hooks.describe_object = [](const SceneDesc& scene, int object) { return tools::object_detail(scene, object); };
  return hooks;
}

}  // namespace redgem
