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
#include "editor/agent/session_history.hpp"

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
  // Where the game's past conversations are kept (empty: none are, and there is no history).
  std::filesystem::path sessions_file;
  // Whether the game's last conversation comes back as the panel opens.
  bool reopen_last = false;
};

// What the panel remembers of an agent's answer already read as markdown: it is read again only
// when more of it came.
struct ParsedText {
  std::size_t length = 0;
  double when = -1.0;  // ImGui's time when it was read
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

  // Takes in what the agent sent; once a frame, whether the panel is shown or not.
  void poll();
  // The dockable window.
  void draw();

  // What its buttons do, for the editor's tests as much as for the buttons.
  void pick(std::size_t agent);
  // A new conversation, or the past one `past_session` names (session/load).
  void start_session(const std::string& past_session = "");
  // A past conversation of the game, with the agent it was had with.
  void open_past(const agent::PastSession& session);
  const std::vector<agent::PastSession>& past_sessions() const { return past_; }
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
  void give_back_queued();
  void take_back_returned();
  bool pickable(std::size_t agent) const;
  bool wants_login() const;
  bool shows_hint() const;
  void send_input();
  void draw_toolbar();
  void draw_history();
  void draw_history_list();
  bool is_current(const agent::PastSession& session) const;
  void note_prompt(const std::string& text);
  bool prompt_now(const std::string& text);
  void draw_agent_picker();
  void draw_status();
  void draw_problem();
  void draw_transcript();
  void draw_entries(agent::AgentClient& client);
  void draw_entry(agent::AgentClient& client, std::size_t index);
  void draw_or_pass(agent::AgentClient& client, std::size_t index);
  void forget_before(std::size_t first);
  void forget_heights_if_resized();
  static void draw_text_entry(const agent::Entry& entry);
  static void answer(agent::AgentClient& client, std::size_t index, const std::optional<std::string>& option);
  void draw_composer();
  void draw_attachments();
  void draw_input();
  void draw_actions();
  void draw_settings();
  void draw_settings_buttons();
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
  std::string returned_;  // a queued prompt given back, put into the box when it is next drawn
  std::string queued_;    // a prompt sent before the session was ready, sent once it is
  std::string hint_;      // a word under the prompt box (nothing selected to attach)
  bool focus_input_ = false;
  std::vector<std::string> attached_;         // context for the next prompt
  std::map<std::size_t, ParsedText> parsed_;  // by entry number
  std::map<std::size_t, float> heights_;      // how tall each entry was last drawn, by number
  float measured_width_ = 0.0f;               // the width those heights were measured at
  std::vector<agent::PastSession> past_;      // the game's past conversations, newest first
  std::string title_;                         // the conversation's, from its first message (empty before)
  AgentSettings settings_;
  bool settings_open_ = false;
};

}  // namespace redgem::ui
