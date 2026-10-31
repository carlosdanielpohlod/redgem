#pragma once

// The messages of the Agent Client Protocol (agentclientprotocol.com, version 1) the editor sends
// and reads, as the editor's own types: the editor is the client, the agent a program it started.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "editor/agent/agent_transcript.hpp"
#include "editor/agent/child_process.hpp"
#include "editor/agent/json_rpc.hpp"

namespace redgem::agent {

inline constexpr int kProtocolVersion = 1;

// The methods, as the protocol names them.
namespace method {
inline constexpr const char* kInitialize = "initialize";
inline constexpr const char* kNewSession = "session/new";
inline constexpr const char* kPrompt = "session/prompt";
inline constexpr const char* kCancel = "session/cancel";
inline constexpr const char* kUpdate = "session/update";
inline constexpr const char* kRequestPermission = "session/request_permission";
inline constexpr const char* kReadTextFile = "fs/read_text_file";
inline constexpr const char* kWriteTextFile = "fs/write_text_file";
}  // namespace method

// An MCP server the agent is to start for itself (stdio): the editor's own tools.
struct McpServer {
  std::string name;
  std::string command;
  std::vector<std::string> arguments;
  std::vector<EnvironmentVariable> environment;
};

// The editor as a client: its name and version, and that it reads and writes the game's text files
// for the agent (it runs no terminals for it).
Json initialize_params(const std::string& client_version);
Json new_session_params(const std::filesystem::path& folder, const std::vector<McpServer>& servers);
// What the user typed, after the context they attached (each its own block of text).
Json prompt_params(const std::string& session, const std::string& text, const std::vector<std::string>& context);
Json cancel_params(const std::string& session);

// What `initialize` answered that the editor uses.
struct AgentInfo {
  int protocol_version = 0;
  std::string name;                        // its title or name, when it said
  std::vector<std::string> login_methods;  // the names of its authentication methods
};
AgentInfo agent_info(const Json& result);

// Puts what a session/update notification says into the transcript; false for an update the editor
// does not show (commands available, mode changes ...) or does not understand.
bool apply_update(Transcript& transcript, const Json& params);

// A session/request_permission request as the question the transcript shows.
PermissionRequest permission_request(const Json& id, const Json& params);
// The answer to it: the option chosen, or that the turn was cancelled.
Json permission_outcome(const PermissionRequest& request);

// What a tool call says (from a tool_call update, or the tool of a permission request).
ToolCallPatch tool_patch(const Json& update);

// What an agent sent is read field by field, each of the type it should be: a field that is missing,
// or of another type, reads as nothing (null, {}, [] or "").
Json field(const Json& value, const char* key);
Json field_object(const Json& value, const char* key);
Json field_list(const Json& value, const char* key);
std::string field_text(const Json& value, const char* key);

// Why a prompt turn ended, as the protocol says it ("end_turn", "cancelled", "refusal" ...).
std::string stop_reason(const Json& result);

}  // namespace redgem::agent
