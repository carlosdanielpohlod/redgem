// The Story panel's runs: a dialogue stepped through and a quest moved on, without Play, on scratch
// GameVariables (StoryPreview, with ruby/engine/editor_preview.rb).

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/config.hpp"
#include "editor/story_preview.hpp"

using namespace redgem;
namespace fs = std::filesystem;

namespace {

constexpr const char* kBaker = R"({"start": "hello", "nodes": {
  "hello": {"speaker": "baker.name", "text": "baker.hello", "effects": [{"set": "gold", "to": 5}],
            "choices": [
              {"text": "baker.buy", "if": [{"var": "gold", "at_least": 5}],
               "effects": [{"add": "gold", "by": -5}, {"quest": "bread", "to": "bring"}], "next": "thanks"},
              {"text": "baker.rich", "if": [{"var": "gold", "at_least": 100}], "end": true}]},
  "thanks": {"text": "baker.thanks", "end": true}}})";

constexpr const char* kBread = R"({"title": "bread.title", "start": "flour", "stages": {
  "flour": {"text": "bread.flour", "objectives": [{"collect": "flour", "count": 3, "text": "bread.collect"}],
            "next": "bring"},
  "bring": {"text": "bread.bring", "next": "done"},
  "done": {"text": "bread.done", "end": true}}})";

constexpr const char* kTexts = R"({"baker.name": "Baker", "baker.hello": "Fresh bread!", "baker.buy": "One, please",
  "baker.rich": "All of it", "baker.thanks": "Thanks!", "bread.title": "Bread", "bread.flour": "Find flour",
  "bread.collect": "Flour {progress}/{count}", "bread.bring": "Bring it", "bread.done": "Done"})";

void write(const fs::path& file, const std::string& text) {
  fs::create_directories(file.parent_path());
  std::ofstream(file) << text;
}

// A game with a dialogue and a quest, and no scene of its own beyond an empty one.
fs::path story_game() {
  const fs::path game = fs::temp_directory_path() / "redgem_story_preview";
  fs::remove_all(game);
  fs::create_directories(game / config::kScriptsDirName);
  write(game / "dialogues/baker.json", kBaker);
  write(game / "quests/bread.json", kBread);
  write(game / "locales/en.json", kTexts);
  return game;
}

const PreviewVariable* variable(const PreviewState& state, const std::string& name) {
  for (const PreviewVariable& found : state.variables) {
    if (found.name == name) return &found;
  }
  return nullptr;
}

}  // namespace

TEST_CASE("a dialogue is stepped through without Play: its texts, conditions and effects") {
  const fs::path game = story_game();
  StoryPreview preview(game);
  REQUIRE_MESSAGE(preview.start_dialogue("baker"), preview.error());
  REQUIRE(preview.state().line);
  const PreviewLine& hello = *preview.state().line;
  CHECK(hello.node == "hello");
  CHECK(hello.speaker == "Baker");
  CHECK(hello.text == "Fresh bread!");
  CHECK(hello.effects == R"([{"set":"gold","to":5}])");
  REQUIRE(hello.choices.size() == 2);
  CHECK(hello.choices[0].offered);
  CHECK_FALSE(hello.choices[1].offered);  // 5 gold is not 100
  CHECK(hello.choices[1].conditions == R"([{"at_least":100,"var":"gold"}])");
  REQUIRE(variable(preview.state(), "gold"));
  CHECK(variable(preview.state(), "gold")->value == "5");

  REQUIRE(preview.choose(0));
  CHECK(preview.state().line->node == "thanks");
  CHECK(variable(preview.state(), "gold")->value == "0");
  REQUIRE(preview.state().quests.size() == 1);
  CHECK(preview.state().quests[0].stage == "bring");  // the choice's effect moved the quest
  REQUIRE(preview.go_on());
  CHECK(preview.state().line->over);
  fs::remove_all(game);
}

TEST_CASE("a quest is moved from stage to stage without Play") {
  const fs::path game = story_game();
  StoryPreview preview(game);
  REQUIRE_MESSAGE(preview.start_quest("bread"), preview.error());
  REQUIRE(preview.state().quests.size() == 1);
  const PreviewQuest& bread = preview.state().quests[0];
  CHECK(bread.stage == "flour");
  CHECK(bread.text == "Find flour");
  CHECK(bread.next == "bring");
  CHECK(bread.stages == std::vector<std::string>{"flour", "bring", "done"});  // as the story goes
  REQUIRE(bread.objectives.size() == 1);
  CHECK(bread.objectives[0].text == "Flour 0/3");
  CHECK_FALSE(bread.objectives[0].done);
  REQUIRE(preview.set_stage("bread", "done"));
  CHECK(preview.state().quests[0].complete);
  preview.reset();
  CHECK(preview.state().quests.empty());
  CHECK(preview.state().variables.empty());
  fs::remove_all(game);
}

TEST_CASE("a step that goes wrong says why and keeps the state") {
  const fs::path game = story_game();
  StoryPreview preview(game);
  REQUIRE(preview.start_quest("bread"));
  CHECK_FALSE(preview.start_dialogue("nobody"));
  CHECK_FALSE(preview.error().empty());
  CHECK_FALSE(preview.set_stage("bread", "nowhere"));
  CHECK(preview.state().quests[0].stage == "flour");
  REQUIRE(preview.start_quest("bread"));
  CHECK(preview.error().empty());
  fs::remove_all(game);
}
