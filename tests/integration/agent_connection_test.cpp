// A real agent program (tests/tools/fake_acp_agent.cpp) talked to over its standard streams: a
// whole turn with streamed text, a tool call, a question answered and a file written in the game.
#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <thread>

#include "editor/agent/agent_connection.hpp"

using namespace redgem::agent;
namespace fs = std::filesystem;

namespace {

constexpr auto kPatience = std::chrono::seconds(20);  // how long the agent gets for what is waited for
constexpr auto kPollEvery = std::chrono::milliseconds(5);

fs::path game_folder() {
  const fs::path folder = fs::temp_directory_path() / "redgem_agent_connection_test";
  fs::remove_all(folder);
  fs::create_directories(folder);
  return folder;
}

ClientSetup setup_in(const fs::path& folder) { return {folder, {{"redgem", "redgem", {"mcp"}, {}}}, "test"}; }

ProgramCall fake_agent(std::vector<EnvironmentVariable> environment = {}) {
  return {REDGEM_FAKE_AGENT_FILE, {}, std::move(environment)};
}

// Polls the connection, as the editor's frames do, until `done` (false when it never was).
bool poll_until(AgentConnection& connection, const std::function<bool()>& done) {
  const auto give_up = std::chrono::steady_clock::now() + kPatience;
  while (std::chrono::steady_clock::now() < give_up) {
    connection.poll();
    if (done()) return true;
    std::this_thread::sleep_for(kPollEvery);
  }
  return false;
}

std::string file_text(const fs::path& file) {
  std::ifstream in(file);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

const Entry* last_of(const Transcript& transcript, EntryKind kind) {
  const auto& entries = transcript.entries();
  for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry) {
    if (entry->kind == kind) return &*entry;
  }
  return nullptr;
}

bool asked(const AgentClient& client) { return !client.transcript().waiting_permissions().empty(); }

}  // namespace

TEST_CASE("a whole turn with an agent program: answer, plan, question, and a file written") {
  const fs::path folder = game_folder();
  AgentConnection connection(fake_agent(), setup_in(folder));
  AgentClient& client = connection.client();
  REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Ready; }));
  CHECK(client.info().name == "Fake agent");
  REQUIRE(client.prompt("remember the coin"));
  REQUIRE(poll_until(connection, [&] { return asked(client); }));
  const Entry* answer = last_of(client.transcript(), EntryKind::Agent);
  REQUIRE(answer != nullptr);
  CHECK(answer->text == "Hello from the **fake** agent.");
  CHECK(last_of(client.transcript(), EntryKind::Thought) != nullptr);
  CHECK(last_of(client.transcript(), EntryKind::Plan) != nullptr);
  CHECK(client.choose(client.transcript().waiting_permissions().front(), "allow"));
  REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Ready; }));
  CHECK(file_text(folder / "notes.txt") == "remember the coin (tools: redgem)\n");
  const Entry* tool = last_of(client.transcript(), EntryKind::Tool);
  REQUIRE(tool != nullptr);
  CHECK(tool->tool.status == ToolStatus::Completed);
  REQUIRE(tool->tool.content.size() == 1);
  CHECK(tool->tool.content[0].change.has_value());
  CHECK(last_of(client.transcript(), EntryKind::Agent)->text.find("Wrote notes.txt.") != std::string::npos);
  fs::remove_all(folder);
}

TEST_CASE("a rejected question leaves the file unwritten, and Stop cancels a waiting one") {
  const fs::path folder = game_folder();
  AgentConnection connection(fake_agent(), setup_in(folder));
  AgentClient& client = connection.client();
  REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Ready; }));
  client.prompt("first");
  REQUIRE(poll_until(connection, [&] { return asked(client); }));
  client.choose(client.transcript().waiting_permissions().front(), "reject");
  REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Ready; }));
  CHECK_FALSE(fs::exists(folder / "notes.txt"));
  CHECK(last_of(client.transcript(), EntryKind::Tool)->tool.status == ToolStatus::Failed);
  client.prompt("second");
  REQUIRE(poll_until(connection, [&] { return asked(client); }));
  client.cancel();
  REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Ready; }));
  CHECK(last_of(client.transcript(), EntryKind::Notice) != nullptr);
  CHECK_FALSE(fs::exists(folder / "notes.txt"));
  fs::remove_all(folder);
}

TEST_CASE("an agent that wants a login fails with its words") {
  const fs::path folder = game_folder();
  AgentConnection connection(fake_agent({{"FAKE_AGENT_LOGIN", "1"}}), setup_in(folder));
  AgentClient& client = connection.client();
  REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Failed; }));
  CHECK(client.needs_login());
  CHECK(client.error() == "fake: log in first");
  fs::remove_all(folder);
}

TEST_CASE("an agent that crashes fails the conversation with what it wrote to its errors") {
  const fs::path folder = game_folder();
  AgentConnection connection(fake_agent(), setup_in(folder));
  AgentClient& client = connection.client();
  REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Ready; }));
  client.prompt("crash");
  REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Failed; }));
  CHECK(client.error().find("crashed on purpose") != std::string::npos);
  fs::remove_all(folder);
}

TEST_CASE("a program that is not there fails at once, saying so") {
  const fs::path folder = game_folder();
  AgentConnection connection({"redgem-no-such-agent", {}, {}}, setup_in(folder));
  CHECK(connection.client().state() == AgentState::Failed);
  CHECK(connection.client().error().find("redgem-no-such-agent") != std::string::npos);
  fs::remove_all(folder);
}

#ifndef _WIN32
TEST_CASE("ending a session does not wait for what the agent started and let go of") {
  if (find_program("python3").empty()) return;  // (what makes the stray process here)
  // A program whose child leaves its process group and holds its streams open for half a minute.
  const std::string stray = "import os, time\nif os.fork() == 0:\n    os.setpgid(0, 0)\ntime.sleep(30)\n";
  const fs::path folder = game_folder();
  const auto started = std::chrono::steady_clock::now();
  {
    AgentConnection connection({"python3", {"-c", stray}, {}}, setup_in(folder));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));  // (time for the child to go its way)
  }
  CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(10));
  fs::remove_all(folder);
}
#endif

TEST_CASE("a conversation with an agent program is loaded again by a new program") {
  const fs::path folder = game_folder();
  std::string session;
  {
    AgentConnection connection(fake_agent(), setup_in(folder));
    AgentClient& client = connection.client();
    REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Ready; }));
    client.prompt("remember the coin");
    REQUIRE(poll_until(connection, [&] { return asked(client); }));
    client.choose(client.transcript().waiting_permissions().front(), "reject");
    REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Ready; }));
    session = client.session_id();
  }
  ClientSetup again = setup_in(folder);
  again.load_session = session;
  AgentConnection connection(fake_agent(), again);
  AgentClient& client = connection.client();
  REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Ready; }));
  const Entry* user = last_of(client.transcript(), EntryKind::User);
  REQUIRE(user != nullptr);
  CHECK(user->text == "remember the coin");
  CHECK(last_of(client.transcript(), EntryKind::Agent)->text == "Hello from the **fake** agent.");
  CHECK(client.session_id() == session);
  fs::remove_all(folder);
}

TEST_CASE("an agent program that cannot load conversations starts a new one") {
  const fs::path folder = game_folder();
  ClientSetup setup = setup_in(folder);
  setup.load_session = "fake-session";
  AgentConnection connection(fake_agent({{"FAKE_AGENT_NO_LOAD", "1"}}), setup);
  AgentClient& client = connection.client();
  REQUIRE(poll_until(connection, [&] { return client.state() == AgentState::Ready; }));
  CHECK(last_of(client.transcript(), EntryKind::Notice) != nullptr);
  CHECK(last_of(client.transcript(), EntryKind::User) == nullptr);
  fs::remove_all(folder);
}
