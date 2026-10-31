// JSON-RPC framing and the Agent Client Protocol messages the editor sends and reads.
#include <doctest/doctest.h>

#include <string>

#include "editor/agent/acp_messages.hpp"
#include "editor/agent/json_rpc.hpp"

using namespace redgem::agent;

namespace {

constexpr std::size_t kSmallLimit = 8;

Json update(const Json& body) { return {{"sessionId", "s1"}, {"update", body}}; }

Json text_block(const std::string& text) { return {{"type", "text"}, {"text", text}}; }

}  // namespace

TEST_CASE("lines come out whole, however the bytes were cut") {
  LineReader reader;
  const std::string bytes = "{\"a\":1}\n{\"b\":";
  reader.feed(bytes.data(), bytes.size());
  CHECK(reader.next() == std::optional<std::string>("{\"a\":1}"));
  CHECK_FALSE(reader.next());
  const std::string rest = "2}\r\n";
  reader.feed(rest.data(), rest.size());
  CHECK(reader.next() == std::optional<std::string>("{\"b\":2}"));
  CHECK_FALSE(reader.next());
  CHECK_FALSE(reader.overflowed());
}

TEST_CASE("a line longer than the limit stops the reading") {
  LineReader reader(kSmallLimit);
  const std::string bytes = "0123456789";
  reader.feed(bytes.data(), bytes.size());
  CHECK_FALSE(reader.next());
  CHECK(reader.overflowed());
}

TEST_CASE("a message is written as one line, its line breaks escaped") {
  const std::string line = to_line(rpc_request(3, "session/prompt", {{"text", "two\nlines"}}));
  CHECK(line.back() == '\n');
  CHECK(line.find('\n') == line.size() - 1);
  CHECK(Json::parse(line)["params"]["text"] == "two\nlines");
}

TEST_CASE("messages are told apart: requests, notifications, responses and errors") {
  CHECK(classify(rpc_request(1, "fs/read_text_file", Json::object())).kind == RpcKind::Request);
  CHECK(classify(rpc_notification("session/update", Json::object())).kind == RpcKind::Notification);
  const RpcMessage result = classify(rpc_result(4, {{"sessionId", "s"}}));
  CHECK(result.kind == RpcKind::Response);
  CHECK(result.id == 4);
  CHECK_FALSE(result.error);
  const RpcMessage failed = classify(rpc_error(5, kAuthRequired, "Authentication required"));
  REQUIRE(failed.error);
  CHECK(failed.error->code == kAuthRequired);
  CHECK(failed.error->message == "Authentication required");
  CHECK(classify(Json::array()).kind == RpcKind::Invalid);
  CHECK(classify({{"jsonrpc", "2.0"}, {"id", 1}}).kind == RpcKind::Invalid);
}

TEST_CASE("initialize says what the editor offers: text files, no terminal") {
  const Json params = initialize_params("0.4.1");
  CHECK(params["protocolVersion"] == kProtocolVersion);
  CHECK(params["clientCapabilities"]["fs"]["readTextFile"] == true);
  CHECK(params["clientCapabilities"]["fs"]["writeTextFile"] == true);
  CHECK(params["clientCapabilities"]["terminal"] == false);
  CHECK(params["clientInfo"]["version"] == "0.4.1");
}

TEST_CASE("a new session works in the game's folder with the editor's MCP server") {
  const McpServer server{"redgem", "/bin/redgem", {"mcp", "/games/demo", "--editor", "/c.json"}, {{"A", "1"}}};
  const Json params = new_session_params("/games/demo", {server});
  CHECK(params["cwd"] == "/games/demo");
  REQUIRE(params["mcpServers"].size() == 1);
  const Json& listed = params["mcpServers"][0];
  CHECK(listed["name"] == "redgem");
  CHECK(listed["command"] == "/bin/redgem");
  CHECK(listed["args"][3] == "/c.json");
  CHECK(listed["env"][0] == Json({{"name", "A"}, {"value", "1"}}));
}

TEST_CASE("a prompt carries the attached context before what was typed") {
  const Json params = prompt_params("s1", "make it spin", {"#3 Coin pos=1,2,3"});
  CHECK(params["sessionId"] == "s1");
  REQUIRE(params["prompt"].size() == 2);
  CHECK(params["prompt"][0] == text_block("#3 Coin pos=1,2,3"));
  CHECK(params["prompt"][1] == text_block("make it spin"));
}

TEST_CASE("what initialize answered: the agent's name, version and ways of logging in") {
  const AgentInfo info = agent_info({{"protocolVersion", 1},
                                     {"agentInfo", {{"name", "claude-agent-acp"}, {"title", "Claude Code"}}},
                                     {"authMethods", {{{"id", "claude-login"}, {"name", "Log in with Claude"}}}}});
  CHECK(info.protocol_version == 1);
  CHECK(info.name == "Claude Code");
  CHECK(info.login_methods == std::vector<std::string>{"Log in with Claude"});
}

TEST_CASE("chunks of an answer and of a thought go on as one entry each") {
  Transcript transcript;
  CHECK(apply_update(transcript, update({{"sessionUpdate", "agent_thought_chunk"}, {"content", text_block("Hm")}})));
  CHECK(apply_update(transcript, update({{"sessionUpdate", "agent_message_chunk"}, {"content", text_block("Hel")}})));
  CHECK(apply_update(transcript, update({{"sessionUpdate", "agent_message_chunk"}, {"content", text_block("lo")}})));
  REQUIRE(transcript.entries().size() == 2);
  CHECK(transcript.entries()[0].kind == EntryKind::Thought);
  CHECK(transcript.entries()[1].text == "Hello");
}

TEST_CASE("a tool call starts, then its updates change only what they say") {
  Transcript transcript;
  apply_update(transcript, update({{"sessionUpdate", "tool_call"},
                                   {"toolCallId", "t1"},
                                   {"title", "Edit scripts/coin.rb"},
                                   {"kind", "edit"},
                                   {"status", "pending"},
                                   {"rawInput", {{"path", "scripts/coin.rb"}}},
                                   {"locations", {{{"path", "/g/scripts/coin.rb"}}}}}));
  apply_update(transcript,
               update({{"sessionUpdate", "tool_call_update"},
                       {"toolCallId", "t1"},
                       {"status", "completed"},
                       {"content",
                        {{{"type", "diff"}, {"path", "/g/scripts/coin.rb"}, {"oldText", "a"}, {"newText", "b"}},
                         {{"type", "content"}, {"content", text_block("done")}}}}}));
  REQUIRE(transcript.entries().size() == 1);
  const ToolCall& call = transcript.entries()[0].tool;
  CHECK(call.title == "Edit scripts/coin.rb");
  CHECK(call.kind == "edit");
  CHECK(call.status == ToolStatus::Completed);
  CHECK(call.input == R"({"path":"scripts/coin.rb"})");
  CHECK(call.locations == std::vector<std::string>{"/g/scripts/coin.rb"});
  REQUIRE(call.content.size() == 2);
  REQUIRE(call.content[0].change);
  REQUIRE(call.content[0].change->lines.size() == 2);  // (the change as lines, worked out once)
  CHECK(call.content[0].change->lines[0].mark == DiffMark::Removed);
  CHECK(call.content[1].text == "done");
}

TEST_CASE("a plan replaces the one before it; updates the editor does not show are passed over") {
  Transcript transcript;
  const Json first = {{"sessionUpdate", "plan"},
                      {"entries", {{{"content", "Read"}, {"priority", "high"}, {"status", "in_progress"}}}}};
  const Json second = {{"sessionUpdate", "plan"},
                       {"entries",
                        {{{"content", "Read"}, {"priority", "high"}, {"status", "completed"}},
                         {{"content", "Write"}, {"priority", "low"}, {"status", "pending"}}}}};
  apply_update(transcript, update(first));
  apply_update(transcript, update(second));
  CHECK_FALSE(apply_update(transcript, update({{"sessionUpdate", "available_commands_update"}})));
  CHECK_FALSE(apply_update(transcript, Json::object()));
  REQUIRE(transcript.entries().size() == 1);
  const auto& plan = transcript.entries()[0].plan;
  REQUIRE(plan.size() == 2);
  CHECK(plan[0].status == PlanStatus::Completed);
  CHECK(plan[1].content == "Write");
}

TEST_CASE("a permission request becomes a question, and its answer the outcome sent back") {
  const Json params = {{"sessionId", "s1"},
                       {"toolCall", {{"toolCallId", "t2"}, {"title", "Run redgem check ."}}},
                       {"options",
                        {{{"optionId", "yes"}, {"name", "Allow"}, {"kind", "allow_once"}},
                         {{"optionId", "always"}, {"name", "Always allow"}, {"kind", "allow_always"}},
                         {{"optionId", "no"}, {"name", "Reject"}, {"kind", "reject_once"}}}}};
  PermissionRequest request = permission_request(7, params);
  CHECK(request.request_id() == 7);
  CHECK(request.tool().title == std::optional<std::string>("Run redgem check ."));
  REQUIRE(request.options().size() == 3);
  CHECK(request.options()[1].kind == PermissionKind::AllowAlways);
  CHECK(request.choose("always"));
  CHECK(permission_outcome(request) == Json({{"outcome", {{"outcome", "selected"}, {"optionId", "always"}}}}));
}

TEST_CASE("a cancelled question answers cancelled") {
  PermissionRequest request(1, {}, {{"yes", "Allow", PermissionKind::AllowOnce}});
  CHECK(request.cancel());
  CHECK(permission_outcome(request) == Json({{"outcome", {{"outcome", "cancelled"}}}}));
}

TEST_CASE("why a turn ended") { CHECK(stop_reason({{"stopReason", "end_turn"}}) == "end_turn"); }

TEST_CASE("a line nested deeper than a message can be is refused") {
  std::string deep;
  for (int level = 0; level < kMaxMessageDepth + 1; ++level) deep += "[";
  for (int level = 0; level < kMaxMessageDepth + 1; ++level) deep += "]";
  CHECK_FALSE(parse_message(deep));
  CHECK_FALSE(parse_message(std::string(100000, '[')));
  CHECK_FALSE(parse_message("not json"));
  const auto fine = parse_message(R"({"jsonrpc":"2.0","method":"x","params":{"a":[[1]]}})");
  REQUIRE(fine);
  CHECK((*fine)["params"]["a"][0][0] == 1);
}

TEST_CASE("a long conversation lets its oldest entries go, but not a question still waiting") {
  Transcript transcript;
  const std::size_t question = transcript.ask(PermissionRequest(1, {}, {{"yes", "Allow", PermissionKind::AllowOnce}}));
  for (std::size_t entry = 0; entry < kMaxEntries * 2; ++entry) transcript.add(EntryKind::Notice, "note");
  CHECK(transcript.entries().size() <= kMaxEntries + 1);
  REQUIRE(transcript.permission(question) != nullptr);
  CHECK(transcript.permission(question)->choose("yes"));
  transcript.add(EntryKind::Notice, "one more");
  CHECK(transcript.entries().size() == kMaxEntries);
  CHECK(transcript.permission(question) == nullptr);  // (let go once answered)
  CHECK(transcript.entries().front().number > question);
  CHECK(transcript.end_number() == kMaxEntries * 2 + 2);
}

TEST_CASE("an option of a kind the editor does not know allows nothing it can tell") {
  const PermissionRequest request = permission_request(
      2, {{"toolCall",
           {{"toolCallId", "t"}, {"rawInput", {{"command", "rm -rf build"}}}, {"locations", {{{"path", "/g/a"}}}}}},
          {"options", {{{"optionId", "x"}, {"name", "Something new"}, {"kind", "allow_for_a_while"}}}}});
  REQUIRE(request.options().size() == 1);
  CHECK(request.options()[0].kind == PermissionKind::Other);
  CHECK(request.tool().input == std::optional<std::string>(R"({"command":"rm -rf build"})"));
  CHECK(request.tool().locations == std::optional<std::vector<std::string>>({"/g/a"}));
}
