// The Story panel runs a dialogue and a quest inside the editor, without Play and without touching
// the edited scene.

#include <doctest/doctest.h>
#include <imgui_internal.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/config.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/editor_app.hpp"
#include "project/editor_setup.hpp"
#include "window_picture.hpp"

using namespace redgem;
namespace {
constexpr int kFrames = 4;
constexpr const char* kDialogue = R"({"start": "hello", "nodes": {
  "hello": {"text": "guard.hello", "choices": [
    {"text": "guard.pass", "if": [{"var": "badge", "is": true}], "next": "open"},
    {"text": "guard.badge", "effects": [{"set": "badge", "to": true}], "next": "hello"}]},
  "open": {"text": "guard.open", "effects": [{"quest": "gate", "to": "inside"}], "end": true}}})";
constexpr const char* kQuest = R"({"title": "gate.title", "start": "outside", "stages": {
  "outside": {"text": "gate.outside"}, "inside": {"text": "gate.inside", "end": true}}})";

void frames(EditorApp& app) {
  for (int frame = 0; frame < kFrames; ++frame) app.frame();
}

void write(const std::filesystem::path& file, const std::string& text) {
  std::filesystem::create_directories(file.parent_path());
  std::ofstream(file) << text;
}

std::filesystem::path story_game() {
  const auto game = std::filesystem::temp_directory_path() / "redgem_story_panel_editor";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / "scripts");
  auto scene = SceneDocument::create_empty(game / config::kSceneFileName);
  std::string error;
  REQUIRE_MESSAGE(scene.save(error), error);
  write(game / "dialogues/guard.json", kDialogue);
  write(game / "quests/gate.json", kQuest);
  return game;
}
}  // namespace

TEST_CASE("the Story panel steps through a dialogue and its quest without Play") {
  const auto game = story_game();
  auto created = EditorApp::create(game, editor_options(GpuChoice::Auto));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  frames(app);
  CHECK(ImGui::FindWindowByName("###story") != nullptr);
  StoryPreview& story = app.story();
  REQUIRE_MESSAGE(story.start_quest("gate"), story.error());
  REQUIRE_MESSAGE(story.start_dialogue("guard"), story.error());
  frames(app);
  CHECK_FALSE(story.state().line->choices[0].offered);  // no badge yet
  REQUIRE(story.choose(1));                             // shows the badge, back to hello
  CHECK(story.state().line->choices[0].offered);
  REQUIRE(story.choose(0));
  frames(app);
  CHECK(story.state().line->node == "open");
  CHECK(story.state().quests[0].stage == "inside");  // the line's effect moved the quest
  CHECK(story.state().quests[0].complete);
  CHECK(test::save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor/story_panel.png"));
  REQUIRE(story.go_on());
  CHECK(story.state().line->over);
  CHECK(app.session().runtime() == nullptr);
  CHECK_FALSE(app.session().document().dirty());
  std::filesystem::remove_all(game);
}
