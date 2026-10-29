// The files an agent reaches through the editor (the game's folder alone), the agents the panel
// offers and how each is started, and where a program is found.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "editor/agent/agent_profiles.hpp"
#include "editor/agent/child_process.hpp"
#include "editor/agent/workspace_files.hpp"
#include "test_support.hpp"

using namespace redgem::agent;
namespace fs = std::filesystem;

namespace {

fs::path fresh_folder(const std::string& name) {
  const fs::path folder = fs::temp_directory_path() / name;
  fs::remove_all(folder);
  fs::create_directories(folder);
  return folder;
}

// A machine where only these programs are installed.
ProgramFinder installed(std::set<std::string> programs) {
  return [programs](const std::string& program) {
    return programs.count(program) > 0 ? fs::path("/usr/bin") / program : fs::path();
  };
}

}  // namespace

TEST_CASE("a workspace reaches the files inside its folder, by whole or relative path") {
  const fs::path folder = fresh_folder("redgem_workspace_inside");
  const Workspace workspace(folder);
  std::string error;
  const auto whole = workspace.resolve((workspace.folder() / "scripts" / "a.rb").u8string(), error);
  REQUIRE(whole);
  CHECK(*whole == workspace.folder() / "scripts" / "a.rb");
  CHECK(workspace.resolve("scripts/../game.json", error) == std::optional<fs::path>(workspace.folder() / "game.json"));
  fs::remove_all(folder);
}

TEST_CASE("a workspace refuses what leaves its folder") {
  const fs::path folder = fresh_folder("redgem_workspace_outside");
  const Workspace workspace(folder);
  redgem::test::LogCapture quiet;
  std::string error;
  CHECK_FALSE(workspace.resolve("../elsewhere.txt", error));
  CHECK(error.find("outside") != std::string::npos);
  CHECK_FALSE(workspace.resolve((workspace.folder() / ".." / "x").u8string(), error));
  CHECK_FALSE(workspace.resolve(workspace.folder().u8string(), error));
  CHECK_FALSE(workspace.resolve((fs::temp_directory_path() / "other").u8string(), error));
  fs::remove_all(folder);
}

TEST_CASE("a workspace refuses a symbolic link, even one to inside it") {
  const fs::path folder = fresh_folder("redgem_workspace_link");
  const fs::path outside = fresh_folder("redgem_workspace_link_target");
  std::error_code link_error;
  fs::create_directory_symlink(outside, folder / "linked", link_error);
  if (!link_error) {  // (Windows without developer mode makes no links)
    const Workspace workspace(folder);
    redgem::test::LogCapture quiet;
    std::string error;
    CHECK_FALSE(workspace.write("linked/escape.txt", "x", error));
    CHECK(error.find("link") != std::string::npos);
    CHECK_FALSE(fs::exists(outside / "escape.txt"));
  }
  fs::remove_all(folder);
  fs::remove_all(outside);
}

TEST_CASE("a workspace writes new files with their folders, and reads lines of them") {
  const fs::path folder = fresh_folder("redgem_workspace_write");
  const Workspace workspace(folder);
  std::string error;
  REQUIRE(workspace.write("scripts/new/spinner.rb", "a\nb\nc\n", error));
  CHECK(workspace.read("scripts/new/spinner.rb", {}, error) == std::optional<std::string>("a\nb\nc\n"));
  CHECK(workspace.read("scripts/new/spinner.rb", {2, std::nullopt}, error) == std::optional<std::string>("b\nc\n"));
  CHECK(workspace.read("scripts/new/spinner.rb", {std::nullopt, 1}, error) == std::optional<std::string>("a\n"));
  CHECK_FALSE(workspace.read("missing.rb", {}, error));
  CHECK_FALSE(workspace.write("big.txt", std::string(kMaxTextFileBytes + 1, 'x'), error));
  fs::remove_all(folder);
}

TEST_CASE("the presets are Claude Code and Codex, through their npm adapters") {
  const auto presets = preset_agents();
  REQUIRE(presets.size() == 2);
  CHECK(presets[0].command == kClaudeCommand);
  CHECK(presets[0].package == kClaudePackage);
  CHECK(presets[1].command == kCodexCommand);
  CHECK_FALSE(login_hint(presets[0]).empty());
  CHECK(login_hint(AgentProfile{"Mine", "mine", {}, {}, ""}).empty());
}

TEST_CASE("an agent starts by its command, else through npx, else says what to install") {
  const AgentProfile claude = preset_agents()[0];
  std::string error;
  const auto direct = launch_call(claude, installed({kClaudeCommand, "npx"}), error);
  REQUIRE(direct);
  CHECK(direct->program == kClaudeCommand);
  const auto through_npx = launch_call(claude, installed({"npx"}), error);
  REQUIRE(through_npx);
  CHECK(through_npx->program == "npx");
  CHECK(through_npx->arguments == std::vector<std::string>{"--yes", kClaudePackage});
  CHECK_FALSE(launch_call(claude, installed({}), error));
  CHECK(error.find(std::string("npm i -g ") + kClaudePackage) != std::string::npos);
  CHECK_FALSE(launch_call(AgentProfile{"Mine", "my-agent", {}, {}, ""}, installed({"npx"}), error));
  CHECK(error.find("my-agent") != std::string::npos);
}

TEST_CASE("the agents are kept in a file, the presets standing in for a missing one") {
  const fs::path folder = fresh_folder("redgem_agents_file");
  const fs::path file = folder / "agents.json";
  CHECK(read_agents(file).size() == preset_agents().size());
  std::vector<AgentProfile> agents = preset_agents();
  agents.push_back({"Mine", "my-agent", {"--acp"}, {{"KEY", "value"}}, ""});
  std::string error;
  REQUIRE(write_agents(file, agents, error));
  const auto read = read_agents(file);
  REQUIRE(read.size() == 3);
  CHECK(read[2].name == "Mine");
  CHECK(read[2].arguments == std::vector<std::string>{"--acp"});
  CHECK(read[2].environment[0].value == "value");
  std::ofstream(file) << "{\"agents\": [{\"name\": 3}]}";
  CHECK(read_agents(file).size() == preset_agents().size());
  fs::remove_all(folder);
}

TEST_CASE("a program is found on the search path, with the system's extensions") {
  const std::set<fs::path> files = {fs::path("/b") / "agent.cmd", fs::path("/c") / "agent"};
  const ProgramTest exists = [&](const fs::path& file) { return files.count(file) > 0; };
  CHECK(find_program_in("agent", "/a:/b:/c", ':', {"", ".cmd"}, exists) == fs::path("/b") / "agent.cmd");
  CHECK(find_program_in("agent", "/a:/c", ':', {""}, exists) == fs::path("/c") / "agent");
  CHECK(find_program_in("agent", "/a", ':', {""}, exists).empty());
  CHECK(find_program_in("/c/agent", "", ':', {""}, exists) == fs::path("/c/agent"));
  CHECK(find_program_in("", "/c", ':', {""}, exists).empty());
}
