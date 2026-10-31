// System test: the editor's Agent panel talking to a coding agent program (tests/tools/
// fake_acp_agent.cpp, which speaks the Agent Client Protocol), with the editor's own MCP server
// given to it and the selection attached to the prompt.
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <thread>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/config.hpp"
#include "editor/editor_app.hpp"
#include "editor/editor_config.hpp"
#include "project/agent_tools.hpp"
#include "project/editor_setup.hpp"
#include "window_mouse.hpp"
#include "window_picture.hpp"

using namespace redgem;

namespace {

constexpr int kWarmupFrames = 10;
constexpr int kPictureFrames = 3;
constexpr auto kPatience = std::chrono::seconds(20);  // how long the agent gets for each step
constexpr auto kFramePause = std::chrono::milliseconds(5);

std::filesystem::path copy_game() {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_editor_agent";
  std::filesystem::remove_all(dir);
  std::filesystem::copy(REDGEM_PLATFORMER_DIR, dir, std::filesystem::copy_options::recursive);
  return dir;
}

// A list of agents with the fake one alone in it.
std::filesystem::path fake_agents_file(std::vector<agent::EnvironmentVariable> environment = {}) {
  const std::filesystem::path file = std::filesystem::temp_directory_path() / "redgem_editor_agents.json";
  std::string error;
  REQUIRE(agent::write_agents(file, {{"Fake agent", REDGEM_FAKE_AGENT_FILE, {}, std::move(environment), ""}}, error));
  return file;
}

void run(EditorApp& app, int frames) {
  for (int i = 0; i < frames; ++i) app.frame();
}

// Frames until `done`, as the editor runs while the agent works (false when it never was).
bool run_until(EditorApp& app, const std::function<bool()>& done) {
  const auto give_up = std::chrono::steady_clock::now() + kPatience;
  while (std::chrono::steady_clock::now() < give_up) {
    app.frame();
    if (done()) return true;
    std::this_thread::sleep_for(kFramePause);
  }
  return false;
}

void take_picture(EditorApp& app, const std::string& name) {
  ImGui::SetWindowFocus(editor_config::kAgentWindow);
  run(app, kPictureFrames);
  CHECK(test::save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor" / (name + ".png")));
}

std::string file_text(const std::filesystem::path& file) {
  std::ifstream in(file);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

constexpr float kPartOfALine = 0.5f;
constexpr int kStreamedPieces = 120;  // pieces of an answer, a frame each: far more than the panel shows
constexpr int kClickFrames = 3;
constexpr const char* kPromptId = "##prompt";

ImGuiWindow* agent_window() { return ImGui::FindWindowByName(editor_config::kAgentWindow); }

// The Agent panel's transcript: its child window.
ImGuiWindow* transcript_window() {
  for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    if (window->ParentWindow == agent_window() && std::strstr(window->Name, "transcript") != nullptr) return window;
  }
  return nullptr;
}

ImGuiID prompt_box() { return ImGui::GetIDWithSeed(kPromptId, nullptr, agent_window()->ID); }

void click_at(EditorApp& app, ImVec2 point) {
  test::move_window_mouse(point.x, point.y);
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(point.x, point.y);
  run(app, kClickFrames);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  run(app, kClickFrames);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  run(app, kClickFrames);
}

// Clicks into the message box, as a user would before typing: it is under the transcript.
void click_into_prompt(EditorApp& app) {
  const ImGuiWindow* transcript = transcript_window();
  REQUIRE(transcript != nullptr);
  const float below = ImGui::GetFrameHeightWithSpacing() * 2.0f;
  click_at(app, ImVec2(transcript->Pos.x + below, transcript->Pos.y + transcript->Size.y + below));
}

// A file of past conversations of the test's own, empty.
std::filesystem::path fresh_sessions_file() {
  const std::filesystem::path file = std::filesystem::temp_directory_path() / "redgem_editor_agent_sessions.json";
  std::filesystem::remove(file);
  return file;
}

// An editor whose Agent panel keeps the game's conversations in `sessions`, and reopens the last.
std::unique_ptr<EditorApp> editor_keeping(const std::filesystem::path& game, const std::filesystem::path& sessions,
                                          std::vector<agent::EnvironmentVariable> agent_environment = {}) {
  EditorOptions options = editor_options(GpuChoice::Auto);
  options.clock = [] { return config::kFixedDeltaSeconds; };
  options.agent.agents_file = fake_agents_file(std::move(agent_environment));
  options.agent.sessions_file = sessions;
  options.agent.reopen_last_session = true;
  auto created = EditorApp::create(game, std::move(options));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<EditorApp>>(created));
}

}  // namespace

TEST_CASE("the editor reopens a game's last conversation with its agent") {
  const std::filesystem::path game = copy_game();
  const std::filesystem::path sessions = fresh_sessions_file();
  {
    auto app = editor_keeping(game, sessions);
    run(*app, kWarmupFrames);
    ui::AgentPanel& panel = app->agent_panel();
    CHECK(panel.connection() == nullptr);  // (nothing to reopen yet)
    REQUIRE(panel.send("keep notes"));
    agent::AgentClient& client = panel.connection()->client();
    REQUIRE(run_until(*app, [&] { return !client.transcript().waiting_permissions().empty(); }));
    client.choose(client.transcript().waiting_permissions().front(), "reject");
    REQUIRE(run_until(*app, [&] { return client.state() == agent::AgentState::Ready; }));
    REQUIRE(panel.past_sessions().size() == 1);
    CHECK(panel.past_sessions()[0].title == "keep notes");
    CHECK(panel.past_sessions()[0].agent == "Fake agent");
  }
  auto app = editor_keeping(game, sessions);  // (the game opened again)
  ui::AgentPanel& panel = app->agent_panel();
  run(*app, kWarmupFrames);
  const bool agent_tab_shown = ImGui::FindWindowByName(editor_config::kAgentWindow)->DockTabIsVisible;
  CHECK_FALSE(agent_tab_shown);          // (behind the Inspector)
  CHECK(panel.connection() == nullptr);  // (unseen: no agent started yet)
  ImGui::SetWindowFocus(editor_config::kAgentWindow);
  run(*app, kPictureFrames);
  REQUIRE(panel.connection() != nullptr);
  agent::AgentClient& client = panel.connection()->client();
  REQUIRE(run_until(*app, [&] { return client.state() == agent::AgentState::Ready; }));
  const auto& entries = client.transcript().entries();
  const auto said = std::find_if(entries.begin(), entries.end(),
                                 [](const agent::Entry& entry) { return entry.kind == agent::EntryKind::User; });
  REQUIRE(said != entries.end());
  CHECK(said->text == "keep notes");
  take_picture(*app, "agent_reopened");
  std::filesystem::remove_all(game);
}

// A conversation the editor keeps with the fake agent: one prompt, its question rejected.
void talk_once(EditorApp& app, const std::string& text) {
  ui::AgentPanel& panel = app.agent_panel();
  ImGui::SetWindowFocus(editor_config::kAgentWindow);
  run(app, kPictureFrames);
  REQUIRE(panel.send(text));
  agent::AgentClient& client = panel.connection()->client();
  REQUIRE(run_until(app, [&] { return !client.transcript().waiting_permissions().empty(); }));
  client.choose(client.transcript().waiting_permissions().front(), "reject");
  REQUIRE(run_until(app, [&] { return client.state() == agent::AgentState::Ready; }));
}

TEST_CASE("a conversation started in place of one that could not be reopened has a title of its own") {
  const std::filesystem::path game = copy_game();
  const std::filesystem::path sessions = fresh_sessions_file();
  {
    auto app = editor_keeping(game, sessions);
    run(*app, kWarmupFrames);
    talk_once(*app, "first thing");
  }
  auto app = editor_keeping(game, sessions, {{"FAKE_AGENT_NO_LOAD", "1"}});  // (it cannot load now)
  run(*app, kWarmupFrames);
  talk_once(*app, "second thing");
  const auto& past = app->agent_panel().past_sessions();
  REQUIRE(past.size() == 2);
  CHECK(past[0].title == "second thing");
  CHECK(past[1].title == "first thing");
  std::filesystem::remove_all(game);
}

TEST_CASE("the transcript follows an answer as it grows, and Play an agent presses leaves the message box alone") {
  const std::filesystem::path game = copy_game();
  EditorOptions options = editor_options(GpuChoice::Auto);
  options.clock = [] { return config::kFixedDeltaSeconds; };
  options.agent.agents_file = fake_agents_file();
  auto created = EditorApp::create(game, std::move(options));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  run(app, kWarmupFrames);
  ui::AgentPanel& panel = app.agent_panel();
  panel.start_session();
  REQUIRE(panel.connection() != nullptr);
  agent::AgentClient& client = panel.connection()->client();
  REQUIRE(run_until(app, [&] { return client.state() == agent::AgentState::Ready; }));
  ImGui::SetWindowFocus(editor_config::kAgentWindow);
  for (int piece = 0; piece < kStreamedPieces; ++piece) {
    client.transcript().append(agent::EntryKind::Agent, "A line of a long answer, as it streams in.\n\n");
    app.frame();
  }
  run(app, kPictureFrames);
  const ImGuiWindow* transcript = transcript_window();
  REQUIRE(transcript != nullptr);
  CHECK(transcript->ScrollMax.y > 0.0f);
  CHECK(transcript->Scroll.y >= transcript->ScrollMax.y);
  // Scrolled up by less than a line, the view stays there: it follows only from the very end.
  // (ImGui keeps scroll positions in whole pixels.)
  const float a_little_up = std::floor(transcript->ScrollMax.y - ImGui::GetTextLineHeight() * kPartOfALine);
  ImGui::SetScrollY(transcript_window(), a_little_up);
  run(app, kPictureFrames);
  CHECK(transcript_window()->Scroll.y == doctest::Approx(a_little_up));

  click_into_prompt(app);
  REQUIRE(ImGui::GetActiveID() == prompt_box());
  app.play();  // (as the editor's MCP tools press it)
  run(app, kPictureFrames);
  CHECK(ImGui::GetActiveID() == prompt_box());
  app.stop();
  std::filesystem::remove_all(game);
}

TEST_CASE("the Agent panel talks to an agent, which writes a file in the game after asking") {
  const std::filesystem::path game = copy_game();
  std::unique_ptr<tools::EditorBridge> bridge;
  EditorOptions options = editor_options(GpuChoice::Auto);
  options.clock = [] { return config::kFixedDeltaSeconds; };
  options.agent = agent_hooks(bridge);
  options.agent.agents_file = fake_agents_file();
  options.agent.sessions_file = fresh_sessions_file();  // (never the user's own)
  auto created = EditorApp::create(game, std::move(options));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  run(app, kWarmupFrames);
  CHECK(ImGui::FindWindowByName(editor_config::kAgentWindow) != nullptr);
  ui::AgentPanel& panel = app.agent_panel();
  REQUIRE(panel.agents().size() == 1);

  app.session().select_by_name("Coin1");
  panel.attach_selection();
  REQUIRE(panel.attached().size() == 1);
  CHECK(panel.attached()[0].find("Coin1") != std::string::npos);

  REQUIRE(panel.send("keep notes"));  // (starts the session, and sends once it is ready)
  agent::AgentConnection* connection = panel.connection();
  REQUIRE(connection != nullptr);
  agent::AgentClient& client = connection->client();
  REQUIRE(run_until(app, [&] { return !client.transcript().waiting_permissions().empty(); }));
  CHECK(panel.attached().empty());
  CHECK(bridge != nullptr);  // (enabled for the agent's tools)
  take_picture(app, "agent_question");

  CHECK(client.choose(client.transcript().waiting_permissions().front(), "allow"));
  REQUIRE(run_until(app, [&] { return client.state() == agent::AgentState::Ready; }));
  CHECK(file_text(game / "notes.txt") == "keep notes (tools: redgem)\n");
  take_picture(app, "agent_answered");
  std::filesystem::remove_all(game);
}

TEST_CASE("the Agent panel says what to install when the agent's command is missing") {
  const std::filesystem::path game = copy_game();
  const std::filesystem::path file = std::filesystem::temp_directory_path() / "redgem_editor_agents_missing.json";
  std::string error;
  REQUIRE(agent::write_agents(file, {{"Missing", "redgem-no-such-agent", {}, {}, ""}}, error));
  EditorOptions options = editor_options(GpuChoice::Auto);
  options.agent.agents_file = file;
  auto created = EditorApp::create(game, std::move(options));
  if (auto* failed = std::get_if<std::string>(&created)) FAIL(*failed);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  run(app, kWarmupFrames);
  app.agent_panel().start_session();
  run(app, kPictureFrames);
  CHECK(app.agent_panel().connection() == nullptr);
  CHECK(app.agent_panel().start_error().find("redgem-no-such-agent") != std::string::npos);
  std::filesystem::remove_all(game);
}
