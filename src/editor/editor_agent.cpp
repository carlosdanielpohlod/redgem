// Part of EditorApp: the Agent panel, given what the editor hands an agent.

#include "editor/editor_app.hpp"

namespace redgem {

void EditorApp::make_agent_panel(AgentHooks hooks) {
  ui::AgentPanelSetup setup;
  setup.game = session_->game_dir();
  setup.agents_file = hooks.agents_file.empty() ? agent::agents_file() : hooks.agents_file;
  setup.client_version = hooks.client_version;
  if (hooks.tool_servers) {
    setup.tool_servers = [this, servers = hooks.tool_servers](std::string& error) { return servers(*this, error); };
  }
  setup.selection_text = [this, describe = hooks.describe_object] { return selection_text(describe); };
  agent_panel_ = std::make_unique<ui::AgentPanel>(std::move(setup));
}

std::vector<std::string> EditorApp::selection_text(
    const std::function<std::string(const SceneDesc&, int)>& describe) const {
  std::vector<std::string> texts;
  if (!describe) return texts;
  for (const ObjectId id : session_->selected().ids()) texts.push_back(describe(session_->scene_desc(), id));
  return texts;
}

}  // namespace redgem
