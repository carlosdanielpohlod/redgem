// A tiny coding agent speaking the Agent Client Protocol over its standard streams, for the tests of
// the editor's Agent panel (no real agent runs in the tests). Every prompt gets the same turn: a
// thought, a streamed answer, a plan, a tool call that asks permission and then writes notes.txt in
// the session's folder through the editor, and reads it back. What it is told changes that:
//   FAKE_AGENT_LOGIN=1     session/new answers "authentication required"
//   prompt "crash"         it writes a line to its standard error and ends
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace {

using Json = nlohmann::json;

constexpr int kProtocolVersion = 1;
constexpr int kAuthRequired = -32000;
constexpr int kCrashExitCode = 3;
constexpr int kPermissionRequest = 100;  // the ids of the agent's own requests
constexpr int kWriteRequest = 101;
constexpr int kReadRequest = 102;
constexpr const char* kSession = "fake-session";
constexpr const char* kNotesFile = "notes.txt";
constexpr const char* kLoginVariable = "FAKE_AGENT_LOGIN";
constexpr const char* kCrashPrompt = "crash";

void send(const Json& message) { std::cout << message.dump() << '\n' << std::flush; }

void answer(const Json& id, const Json& result) { send({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}}); }

void ask(int id, const std::string& method, const Json& params) {
  send({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}});
}

void update(const Json& body) {
  send({{"jsonrpc", "2.0"}, {"method", "session/update"}, {"params", {{"sessionId", kSession}, {"update", body}}}});
}

void say(const char* kind, const std::string& text) {
  update({{"sessionUpdate", kind}, {"content", {{"type", "text"}, {"text", text}}}});
}

std::optional<Json> read_message() {
  std::string line;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    Json message = Json::parse(line, nullptr, false);
    if (!message.is_discarded()) return message;
  }
  return std::nullopt;
}

// Reads on until the answer to the agent's request `id` (a cancellation noticed meanwhile is the
// editor answering the question "cancelled", which comes too).
Json wait_for_answer(int id) {
  while (const auto message = read_message()) {
    if (message->value("id", Json()) == id && !message->contains("method")) return *message;
  }
  std::exit(EXIT_SUCCESS);
}

struct Session {
  std::string folder;
  std::string tools;  // the name of the first MCP server the editor gave
};

void tool_status(const char* status, const Json& content = Json::array()) {
  update({{"sessionUpdate", "tool_call_update"}, {"toolCallId", "t1"}, {"status", status}, {"content", content}});
}

Json permission_params() {
  return {{"sessionId", kSession},
          {"toolCall", {{"toolCallId", "t1"}, {"title", "Write notes.txt"}}},
          {"options",
           {{{"optionId", "allow"}, {"name", "Allow"}, {"kind", "allow_once"}},
            {{"optionId", "always"}, {"name", "Always allow"}, {"kind", "allow_always"}},
            {{"optionId", "reject"}, {"name", "Reject"}, {"kind", "reject_once"}}}}};
}

// Writes notes.txt through the editor and reads it back; true when it reads what was written.
bool write_notes(const Session& session, const std::string& text) {
  const std::string path = session.folder + "/" + kNotesFile;
  ask(kWriteRequest, "fs/write_text_file", {{"sessionId", kSession}, {"path", path}, {"content", text}});
  if (!wait_for_answer(kWriteRequest).contains("result")) return false;
  ask(kReadRequest, "fs/read_text_file", {{"sessionId", kSession}, {"path", path}});
  const Json read = wait_for_answer(kReadRequest);
  return read.contains("result") && read["result"].value("content", std::string()) == text;
}

const char* acted_on(const Session& session, const Json& outcome, const std::string& notes) {
  if (outcome.value("outcome", std::string()) == "cancelled") return "cancelled";
  if (outcome.value("optionId", std::string()) == "reject") {
    tool_status("failed");
    return "end_turn";
  }
  const bool wrote = write_notes(session, notes);
  tool_status(wrote ? "completed" : "failed",
              Json::array({{{"type", "diff"}, {"path", session.folder + "/" + kNotesFile}, {"newText", notes}}}));
  say("agent_message_chunk", wrote ? " Wrote notes.txt." : " Could not write notes.txt.");
  return "end_turn";
}

void prompt(const Json& id, const Json& params, const Session& session) {
  const std::string text = params["prompt"].back().value("text", std::string());
  if (text == kCrashPrompt) {
    std::cerr << "fake agent: crashed on purpose" << std::endl;
    std::exit(kCrashExitCode);
  }
  say("agent_thought_chunk", "Thinking about it.");
  say("agent_message_chunk", "Hello ");
  say("agent_message_chunk", "from the **fake** agent.");
  update({{"sessionUpdate", "plan"},
          {"entries", {{{"content", "Write the notes"}, {"priority", "high"}, {"status", "in_progress"}}}}});
  update({{"sessionUpdate", "tool_call"},
          {"toolCallId", "t1"},
          {"title", "Write notes.txt"},
          {"kind", "edit"},
          {"status", "pending"}});
  ask(kPermissionRequest, "session/request_permission", permission_params());
  const Json outcome = wait_for_answer(kPermissionRequest)["result"]["outcome"];
  answer(id, {{"stopReason", acted_on(session, outcome, text + " (tools: " + session.tools + ")\n")}});
}

void open_session(const Json& id, const Json& params, Session& session) {
  if (std::getenv(kLoginVariable) != nullptr) {
    send({{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", kAuthRequired}, {"message", "fake: log in first"}}}});
    return;
  }
  session.folder = params.value("cwd", std::string());
  const Json servers = params.value("mcpServers", Json::array());
  session.tools = servers.empty() ? "none" : servers[0].value("name", std::string());
  answer(id, {{"sessionId", kSession}});
}

}  // namespace

int main() {
  Session session;
  while (const auto message = read_message()) {
    const std::string method = message->value("method", std::string());
    const Json id = message->value("id", Json());
    const Json params = message->value("params", Json::object());
    if (method == "initialize") {
      answer(id,
             {{"protocolVersion", kProtocolVersion}, {"agentInfo", {{"name", "fake-agent"}, {"title", "Fake agent"}}}});
    } else if (method == "session/new") {
      open_session(id, params, session);
    } else if (method == "session/prompt") {
      prompt(id, params, session);
    }
  }
  return EXIT_SUCCESS;
}
