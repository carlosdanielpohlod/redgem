#pragma once

// The Agent panel: a conversation with a coding agent (Claude Code, Codex, or one of the user's
// own) beside the scene, as a coding assistant's chat shows it. The agent runs as its own program
// through an Agent Client Protocol adapter, works in the game's folder, and gets the editor's MCP
// tools to see and change the open scene.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <memory>
#include <string>
#include <vector>

#include "editor/agent/agent_connection.hpp"
#include "editor/agent/agent_profiles.hpp"
#include "editor/agent/markdown_blocks.hpp"

namespace redgem::ui {

// What the panel is given by the editor.
struct AgentPanelSetup {
  std::filesystem::path game;         // the folder the agent works in
  std::filesystem::path agents_file;  // where the list of agents is kept
  std::string client_version;
  // The MCP servers a new session gets (the editor's live tools); empty, with why, when none.
  std::function<std::vector<agent::McpServer>(std::string& error)> tool_servers;
  // The selected objects as `redgem scene get` describes them, one text each.
  std::function<std::vector<std::string>()> selection_text;
  agent::ProgramFinder find = agent::find_program;
};

// What the panel remembers of an agent's answer already read as markdown: it is read again only
// when more of it came.
struct ParsedText {
  std::size_t length = 0;
  std::vector<agent::Block> blocks;
};

// The settings popup's copy of the agents, edited before it is kept.
struct AgentSettings {
  std::vector<agent::AgentProfile> agents;
  std::size_t selected = 0;
  std::string arguments;    // the selected agent's, a line each
  std::string environment;  // NAME=value, a line each
  std::string error;
};

class AgentPanel {
 public:
  explicit AgentPanel(AgentPanelSetup setup);
  ~AgentPanel();
  AgentPanel(const AgentPanel&) = delete;
  AgentPanel& operator=(const AgentPanel&) = delete;

  // The dockable window, once a frame; takes in what the agent sent first.
  void draw();

  // What its buttons do, for the editor's tests as much as for the buttons.
  void pick(std::size_t agent);
  void start_session();
  bool send(const std::string& text);
  void stop();
  void attach_selection();
  void end_session();

  const std::vector<agent::AgentProfile>& agents() const { return agents_; }
  std::size_t picked() const { return picked_; }
  // The conversation running (nullptr before the first session, or after a failed start's error).
  agent::AgentConnection* connection() { return connection_.get(); }
  // Why a session could not start (the adapter missing, the editor's tools unavailable).
  const std::string& start_error() const { return start_error_; }
  const std::vector<std::string>& attached() const { return attached_; }

 private:
  void poll();
  void draw_toolbar();
  void draw_agent_picker();
  void draw_status();
  void draw_problem();
  void draw_transcript();
  void follow_new_entries(bool at_bottom);
  void draw_entries(agent::AgentClient& client);
  void draw_entry(agent::AgentClient& client, std::size_t index);
  static void answer(agent::AgentClient& client, std::size_t index, const std::optional<std::string>& option);
  void draw_failure(const std::string& text);
  void draw_composer();
  void draw_attachments();
  void draw_input();
  void draw_actions();
  void draw_settings();
  void open_settings();
  void keep_settings();
  bool can_send() const;
  bool running() const;
  bool live() const;
  const std::vector<agent::Block>& markdown(std::size_t entry, const std::string& text);

  AgentPanelSetup setup_;
  std::vector<agent::AgentProfile> agents_;
  std::size_t picked_ = 0;
  std::unique_ptr<agent::AgentConnection> connection_;
  std::string start_error_;
  std::string input_;
  std::string queued_;  // a prompt sent before the session was ready, sent once it is
  std::string hint_;    // a word under the prompt box (nothing selected to attach)
  bool focus_input_ = false;
  std::vector<std::string> attached_;  // context for the next prompt
  std::uint64_t seen_revision_ = 0;   // the transcript's, as last drawn
  std::map<std::size_t, ParsedText> parsed_;
  AgentSettings settings_;
  bool settings_open_ = false;
};

}  // namespace redgem::ui
