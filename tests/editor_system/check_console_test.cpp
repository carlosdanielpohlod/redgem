// File > Check Game puts what `redgem check` finds in the Console, a script's error linked to its line.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "core/config.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/editor_app.hpp"
#include "editor/scripts/script_workspace.hpp"
#include "project/editor_setup.hpp"

using namespace redgem;
namespace {
constexpr int kFrames = 3;
constexpr std::size_t kSyntaxErrorLine = 5;

void frames(EditorApp& app) {
  for (int frame = 0; frame < kFrames; ++frame) app.frame();
}

std::filesystem::path broken_game() {
  const auto game = std::filesystem::temp_directory_path() /
                    ("redgem_check_console_editor_" + std::to_string(std::random_device{}()));
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / "scripts");
  auto scene = SceneDocument::create_empty(game / config::kSceneFileName);
  std::string error;
  REQUIRE_MESSAGE(scene.save(error), error);
  std::ofstream(game / "scripts/coin.rb") << "class Coin < Behaviour\n  def update(dt)\n    if true\n  end\n";
  return game;
}

// The Console's entry that holds a text, or nothing.
const ConsoleEntry* entry_with(const ConsoleLog& console, const std::string& text) {
  for (const ConsoleEntry& entry : console.entries()) {
    if (entry.message.find(text) != std::string::npos) return &entry;
  }
  return nullptr;
}
}  // namespace

TEST_CASE("Check Game shows the check's errors in the Console, linked to the script's line") {
  const auto game = broken_game();
  auto created = EditorApp::create(game, editor_options(GpuChoice::Auto));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  frames(app);
  const auto errors_before = app.console().error_count();
  REQUIRE(app.check_game());
  frames(app);
  CHECK(app.console().error_count() == errors_before + 1);
  const ConsoleEntry* error = entry_with(app.console(), "scripts/coin.rb:5: syntax error");
  REQUIRE(error != nullptr);
  CHECK(error->level == log::Level::Error);
  REQUIRE(error->rows.size() == 2);
  REQUIRE(error->rows[1].source);
  CHECK(error->rows[1].source->line == kSyntaxErrorLine);
  ScriptWorkspace scripts(game / config::kScriptsDirName);
  std::string problem;
  CHECK_MESSAGE(scripts.open_at(std::filesystem::u8path(error->rows[1].source->file), kSyntaxErrorLine, problem),
                problem);
  CHECK(entry_with(app.console(), "check: 1 errors, 0 warnings") != nullptr);
  CHECK_FALSE(app.session().document().dirty());
  std::filesystem::remove_all(game);
}

TEST_CASE("an editor opened with no check offers none") {
  const auto game = broken_game();
  auto created = EditorApp::create(game, EditorOptions{});
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  frames(app);
  CHECK_FALSE(app.check_game());
  std::filesystem::remove_all(game);
}
