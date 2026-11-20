// What the Story panel reads of the state its preview engine tells (preview_state_from).

#include <doctest/doctest.h>

#include "editor/story_preview.hpp"

using namespace redgem;

TEST_CASE("the preview's state is read from what the engine tells") {
  const auto json = nlohmann::json::parse(R"({
    "dialogue": {"name": "baker", "node": "hello", "over": false, "speaker": "Baker", "text": "Hi",
                 "text_key": "baker.hello", "effects": [], "next": null,
                 "choices": [{"index": 0, "text": "Buy", "if": [{"var": "gold", "at_least": 5}],
                              "effects": [], "next": "thanks", "offered": true}]},
    "quests": [{"name": "bread", "stage": "flour", "complete": false, "text": "Find flour", "next": "bring",
                "stages": ["bring", "flour"], "objectives": [{"text": "Flour 1/3", "done": false}]}],
    "variables": {"gold": 5, "has_bread": true}})");
  const PreviewState state = preview_state_from(json);
  REQUIRE(state.line);
  CHECK(state.line->speaker == "Baker");
  CHECK(state.line->next == "null");
  REQUIRE(state.line->choices.size() == 1);
  CHECK(state.line->choices[0].conditions == R"([{"at_least":5,"var":"gold"}])");
  CHECK(state.line->choices[0].next == "\"thanks\"");
  CHECK(state.line->choices[0].offered);
  REQUIRE(state.quests.size() == 1);
  CHECK(state.quests[0].stages.size() == 2);
  CHECK(state.quests[0].objectives[0].text == "Flour 1/3");
  REQUIRE(state.variables.size() == 2);
  CHECK(state.variables[0].name == "gold");
  CHECK(state.variables[1].value == "true");
}

TEST_CASE("a state with no conversation, or that is no state, shows nothing") {
  CHECK_FALSE(preview_state_from(nlohmann::json::parse(R"({"dialogue": null, "quests": [], "variables": {}})")).line);
  const PreviewState nothing = preview_state_from(nlohmann::json::parse("[1, 2]"));
  CHECK_FALSE(nothing.line);
  CHECK(nothing.quests.empty());
  CHECK(nothing.variables.empty());
}
