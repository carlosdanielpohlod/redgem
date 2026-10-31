// The editor's side of a conversation with an agent, driven message by message: what it sends, how
// it follows the agent's answers, questions and file requests, and when it stops.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "editor/agent/agent_client.hpp"

using namespace redgem::agent;
namespace fs = std::filesystem;

namespace {

// A client whose messages are kept, in a game folder of its own.
struct Conversation {
  Conversation() : folder(fs::temp_directory_path() / "redgem_agent_client_test") {
    fs::remove_all(folder);
    fs::create_directories(folder / "scripts");
    std::ofstream(folder / "scripts" / "coin.rb", std::ios::binary) << "one\ntwo\nthree\n";
    client = std::make_unique<AgentClient>(ClientSetup{folder, {{"redgem", "redgem", {"mcp"}, {}}}, "0.4.1"},
                                           [this](const Json& message) { sent.push_back(message); });
  }
  ~Conversation() { fs::remove_all(folder); }
  Conversation(const Conversation&) = delete;
  Conversation& operator=(const Conversation&) = delete;

  const Json& last() const { return sent.back(); }
  int last_id() const { return last()["id"].get<int>(); }

  // Up to Ready: initialize and session/new answered.
  void open() {
    client->begin();
    client->receive(rpc_result(last_id(), {{"protocolVersion", 1}}));
    client->receive(rpc_result(last_id(), {{"sessionId", "s1"}}));
  }

  Json request(int id, const std::string& method, const Json& params) {
    client->receive(rpc_request(id, method, params));
    return last();
  }

  fs::path folder;
  std::vector<Json> sent;
  std::unique_ptr<AgentClient> client;
};

constexpr int kAgentsRequest = 90;  // an id the agent gives its own request

Json permission_params() {
  return {{"sessionId", "s1"},
          {"toolCall", {{"toolCallId", "t1"}, {"title", "Write scripts/coin.rb"}}},
          {"options",
           {{{"optionId", "allow"}, {"name", "Allow"}, {"kind", "allow_once"}},
            {{"optionId", "reject"}, {"name", "Reject"}, {"kind", "reject_once"}}}}};
}

}  // namespace

TEST_CASE("the client initializes, opens a session in the game's folder and is then ready") {
  Conversation talk;
  talk.client->begin();
  CHECK(talk.client->state() == AgentState::Initializing);
  CHECK(talk.last()["method"] == "initialize");
  talk.client->receive(rpc_result(talk.last_id(), {{"protocolVersion", 1}, {"agentInfo", {{"name", "fake"}}}}));
  CHECK(talk.client->state() == AgentState::OpeningSession);
  CHECK(talk.last()["method"] == "session/new");
  CHECK(talk.last()["params"]["cwd"] == talk.client->workspace().folder().u8string());
  CHECK(talk.last()["params"]["mcpServers"][0]["name"] == "redgem");
  talk.client->receive(rpc_result(talk.last_id(), {{"sessionId", "s1"}}));
  CHECK(talk.client->state() == AgentState::Ready);
  CHECK(talk.client->info().name == "fake");
}

TEST_CASE("an agent of another protocol version is refused") {
  Conversation talk;
  talk.client->begin();
  talk.client->receive(rpc_result(talk.last_id(), {{"protocolVersion", 2}}));
  CHECK(talk.client->state() == AgentState::Failed);
  CHECK(talk.client->error().find('2') != std::string::npos);
}

TEST_CASE("an agent that wants its user to log in fails with its own words") {
  Conversation talk;
  talk.client->begin();
  talk.client->receive(rpc_result(talk.last_id(), {{"protocolVersion", 1}}));
  talk.client->receive(rpc_error(talk.last_id(), kAuthRequired, "Please run /login"));
  CHECK(talk.client->state() == AgentState::Failed);
  CHECK(talk.client->needs_login());
  CHECK(talk.client->error() == "Please run /login");
  CHECK(talk.client->transcript().entries().back().kind == EntryKind::Error);
}

TEST_CASE("a turn: the prompt, the streamed answer, and ready again when it ends") {
  Conversation talk;
  talk.open();
  CHECK_FALSE(talk.client->busy());
  CHECK(talk.client->prompt("hello", {"context"}));
  CHECK(talk.client->busy());
  CHECK_FALSE(talk.client->prompt("again"));  // (one turn at a time)
  const int turn = talk.last_id();
  CHECK(talk.last()["params"]["prompt"].size() == 2);
  talk.client->receive(rpc_notification(
      "session/update",
      {{"sessionId", "s1"},
       {"update", {{"sessionUpdate", "agent_message_chunk"}, {"content", {{"type", "text"}, {"text", "hi"}}}}}}));
  talk.client->receive(rpc_result(turn, {{"stopReason", "end_turn"}}));
  CHECK(talk.client->state() == AgentState::Ready);
  const auto& entries = talk.client->transcript().entries();
  REQUIRE(entries.size() == 2);
  CHECK(entries[0].kind == EntryKind::User);
  CHECK(entries[1].text == "hi");
}

TEST_CASE("a question waits for one answer, which goes back to the agent") {
  Conversation talk;
  talk.open();
  talk.client->prompt("change the coin");
  talk.client->receive(rpc_request(kAgentsRequest, "session/request_permission", permission_params()));
  const std::size_t question = talk.client->transcript().entries().size() - 1;
  CHECK(talk.client->transcript().waiting_permissions() == std::vector<std::size_t>{question});
  CHECK_FALSE(talk.client->choose(question, "no-such-option"));
  CHECK(talk.client->choose(question, "allow"));
  CHECK(talk.last() == rpc_result(kAgentsRequest, {{"outcome", {{"outcome", "selected"}, {"optionId", "allow"}}}}));
  CHECK_FALSE(talk.client->choose(question, "reject"));  // (answered once)
  CHECK(talk.client->transcript().waiting_permissions().empty());
}

TEST_CASE("stopping a turn cancels it and answers the questions still waiting") {
  Conversation talk;
  talk.open();
  talk.client->prompt("change the coin");
  const int turn = talk.last_id();
  talk.client->receive(rpc_request(kAgentsRequest, "session/request_permission", permission_params()));
  talk.client->cancel();
  REQUIRE(talk.sent.size() >= 2);
  CHECK(talk.sent[talk.sent.size() - 2]["method"] == "session/cancel");
  CHECK(talk.last() == rpc_result(kAgentsRequest, {{"outcome", {{"outcome", "cancelled"}}}}));
  talk.client->receive(rpc_result(turn, {{"stopReason", "cancelled"}}));
  CHECK(talk.client->state() == AgentState::Ready);
  CHECK(talk.client->transcript().entries().back().kind == EntryKind::Notice);
}

TEST_CASE("the agent reads and writes the game's files, and nothing outside them") {
  Conversation talk;
  talk.open();
  const std::string coin = (talk.client->workspace().folder() / "scripts" / "coin.rb").u8string();
  const Json read = talk.request(kAgentsRequest, "fs/read_text_file",
                                 {{"sessionId", "s1"}, {"path", coin}, {"line", 2}, {"limit", 1}});
  CHECK(read["result"]["content"] == "two\n");
  const Json wrote =
      talk.request(kAgentsRequest, "fs/write_text_file", {{"sessionId", "s1"}, {"path", coin}, {"content", "four\n"}});
  CHECK(wrote.contains("result"));
  CHECK(talk.request(kAgentsRequest, "fs/read_text_file", {{"sessionId", "s1"}, {"path", coin}})["result"]["content"] ==
        "four\n");
  const std::string outside = (talk.folder.parent_path() / "elsewhere.txt").u8string();
  const Json refused =
      talk.request(kAgentsRequest, "fs/write_text_file", {{"sessionId", "s1"}, {"path", outside}, {"content", "x"}});
  CHECK(refused["error"]["code"] == kInternalError);
  CHECK_FALSE(fs::exists(outside));
  CHECK(talk.request(kAgentsRequest, "fs/read_text_file", {{"sessionId", "s1"}})["error"]["code"] == kInvalidParams);
}

TEST_CASE("a request the editor does not know is answered as such") {
  Conversation talk;
  talk.open();
  CHECK(talk.request(kAgentsRequest, "terminal/create", Json::object())["error"]["code"] == kMethodNotFound);
}

TEST_CASE("an agent that went away fails the conversation once, giving up its questions") {
  Conversation talk;
  talk.open();
  talk.client->prompt("go");
  talk.client->receive(rpc_request(kAgentsRequest, "session/request_permission", permission_params()));
  talk.client->fail("it ended");
  talk.client->fail("again");
  CHECK(talk.client->state() == AgentState::Failed);
  CHECK(talk.client->error() == "it ended");
  CHECK(talk.client->transcript().waiting_permissions().empty());
}

TEST_CASE("answers of the wrong shape fail the conversation instead of the editor") {
  const std::vector<Json> broken = {Json(),                // a null result
                                    {{"sessionId", 7}},    // a numeric session id
                                    Json::array({1, 2})};  // an array
  for (const Json& result : broken) {
    Conversation talk;
    talk.client->begin();
    talk.client->receive(rpc_result(talk.last_id(), {{"protocolVersion", 1}}));
    CHECK_NOTHROW(talk.client->receive(rpc_result(talk.last_id(), result)));
    CHECK(talk.client->state() == AgentState::Failed);
  }
  Conversation talk;
  talk.client->begin();
  const Json odd_error = {{"jsonrpc", "2.0"}, {"id", talk.last_id()}, {"error", {{"code", "x"}, {"message", 3}}}};
  CHECK_NOTHROW(talk.client->receive(odd_error));
  CHECK(talk.client->state() == AgentState::Failed);
}

TEST_CASE("requests and updates of the wrong shape do not reach past the client") {
  Conversation talk;
  talk.open();
  CHECK_NOTHROW(talk.client->receive(rpc_request(kAgentsRequest, "session/request_permission", Json::array())));
  CHECK_NOTHROW(talk.client->receive(
      rpc_request(kAgentsRequest, "session/request_permission", {{"toolCall", 5}, {"options", 3}})));
  CHECK_NOTHROW(talk.client->receive(rpc_notification("session/update", Json::array())));
  CHECK_NOTHROW(talk.client->receive(
      rpc_notification("session/update", {{"update", {{"sessionUpdate", "plan"}, {"entries", 4}}}})));
  CHECK_NOTHROW(talk.client->receive(rpc_notification(
      "session/update", {{"update", {{"sessionUpdate", "tool_call"}, {"content", 1}, {"locations", "x"}}}})));
}

TEST_CASE("a question that comes after Stop is answered cancelled at once") {
  Conversation talk;
  talk.open();
  talk.client->prompt("go");
  const int turn = talk.last_id();
  talk.client->cancel();
  talk.client->receive(rpc_request(kAgentsRequest, "session/request_permission", permission_params()));
  CHECK(talk.last() == rpc_result(kAgentsRequest, {{"outcome", {{"outcome", "cancelled"}}}}));
  CHECK(talk.client->transcript().waiting_permissions().empty());
  talk.client->receive(rpc_result(turn, {{"stopReason", "cancelled"}}));
  CHECK(talk.client->prompt("again"));
  talk.client->receive(rpc_request(kAgentsRequest + 1, "session/request_permission", permission_params()));
  CHECK_FALSE(talk.client->transcript().waiting_permissions().empty());  // (a new turn asks again)
}

TEST_CASE("a turn that ends with a question still open answers it cancelled") {
  Conversation talk;
  talk.open();
  talk.client->prompt("go");
  const int turn = talk.last_id();
  talk.client->receive(rpc_request(kAgentsRequest, "session/request_permission", permission_params()));
  talk.client->receive(rpc_result(turn, {{"stopReason", "end_turn"}}));
  CHECK(talk.last() == rpc_result(kAgentsRequest, {{"outcome", {{"outcome", "cancelled"}}}}));
}
