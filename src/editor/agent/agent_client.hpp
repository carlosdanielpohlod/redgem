#pragma once

// The editor's side of a conversation with a coding agent over the Agent Client Protocol, apart from
// how the messages travel: it is handed each message the agent sent and hands back the ones to send.
// initialize, then session/new (with the editor's MCP server), then a prompt turn at a time, with
// the agent's updates, questions and file requests answered as they come.

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "editor/agent/acp_messages.hpp"
#include "editor/agent/agent_transcript.hpp"
#include "editor/agent/workspace_files.hpp"

namespace redgem::agent {

enum class AgentState {
  Starting,        // nothing sent yet
  Initializing,    // initialize sent
  OpeningSession,  // session/new sent
  Ready,           // waiting for the user
  Prompting,       // a turn is running
  Failed,          // it cannot go on: error() says why
};

// What the client is set up with.
struct ClientSetup {
  std::filesystem::path folder;  // the game's: the session's working folder, and the only files it reaches
  std::vector<McpServer> servers;
  std::string client_version;
};

class AgentClient {
 public:
  using Send = std::function<void(const Json& message)>;
  AgentClient(ClientSetup setup, Send send);

  // Sends initialize.
  void begin();
  // One message the agent sent.
  void receive(const Json& message);
  // Starts a turn with what the user typed and the context they attached; false unless Ready.
  bool prompt(const std::string& text, const std::vector<std::string>& context = {});
  // Stops the turn running: session/cancel, and every question still waiting answered "cancelled".
  void cancel();
  // Answers the question at `entry` of the transcript with one of its options.
  bool choose(std::size_t entry, const std::string& option_id);
  // The agent cannot be talked to any more (its program ended, its output could not be read).
  void fail(const std::string& why);

  AgentState state() const { return state_; }
  bool busy() const { return state_ == AgentState::Prompting; }
  const std::string& error() const { return error_; }
  // Whether the agent asked its user to log in (session/new answered "authentication required").
  bool needs_login() const { return needs_login_; }
  const AgentInfo& info() const { return info_; }
  const Transcript& transcript() const { return transcript_; }
  Transcript& transcript() { return transcript_; }
  const Workspace& workspace() const { return workspace_; }

 private:
  // What a request sent waits for.
  enum class Call { Initialize, NewSession, Prompt };
  int request(Call call, const char* method, const Json& params);
  void on_response(const RpcMessage& message);
  void on_request(const RpcMessage& message);
  void on_notification(const RpcMessage& message);
  void on_initialized(const RpcMessage& message);
  void on_session(const RpcMessage& message);
  void on_turn_end(const RpcMessage& message);
  void answer(const Json& id, const Json& result) { send_(rpc_result(id, result)); }
  void refuse(const Json& id, int code, const std::string& why) { send_(rpc_error(id, code, why)); }
  void read_file(const RpcMessage& message);
  void write_file(const RpcMessage& message);
  void ask_permission(const RpcMessage& message);
  void answer_permission(PermissionRequest& request);
  void say_why_the_turn_ended(const std::string& reason);

  ClientSetup setup_;
  Send send_;
  Workspace workspace_;
  Transcript transcript_;
  AgentState state_ = AgentState::Starting;
  std::string error_;
  bool needs_login_ = false;
  AgentInfo info_;
  std::string session_;
  int next_id_ = 0;
  std::map<int, Call> waiting_;  // requests sent, by id, until answered
};

}  // namespace redgem::agent
