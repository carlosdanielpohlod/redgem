// System test: the editor's Agent panel talking to a coding agent program (tests/tools/
// fake_acp_agent.cpp, which speaks the Agent Client Protocol), with the editor's own MCP server
// given to it and the selection attached to the prompt.
#include <doctest/doctest.h>

#include <chrono>
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
std::filesystem::path fake_agents_file() {
  const std::filesystem::path file = std::filesystem::temp_directory_path() / "redgem_editor_agents.json";
  std::string error;
  REQUIRE(agent::write_agents(file, {{"Fake agent", REDGEM_FAKE_AGENT_FILE, {}, {}, ""}}, error));
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

}  // namespace

TEST_CASE("the Agent panel talks to an agent, which writes a file in the game after asking") {
  const std::filesystem::path game = copy_game();
  std::unique_ptr<tools::EditorBridge> bridge;
  EditorOptions options = editor_options(GpuChoice::Auto);
  options.clock = [] { return config::kFixedDeltaSeconds; };
  options.agent = agent_hooks(bridge);
  options.agent.agents_file = fake_agents_file();
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
