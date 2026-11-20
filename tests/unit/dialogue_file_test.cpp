// Dialogue files looked over as the engine reads one and `redgem check` reads them all: what makes
// one unusable is an error, what may be a mistake a warning, and the texts and quests it names are
// listed for the check.

#include <doctest/doctest.h>

#include <algorithm>
#include <string>

#include "script/dialogue_file.hpp"

using namespace redgem;
using namespace redgem::dialogue;

namespace {

// A baker who sells bread to a player with gold, and says goodbye.
constexpr const char* kBaker = R"({
  "start": [{ "if": [{ "var": "met_baker", "is": true }], "next": "again" }, { "next": "hello" }],
  "nodes": {
    "hello": { "speaker": "baker.name", "text": "baker.hello", "voice": "baker/hello",
               "effects": [{ "set": "met_baker", "to": true }], "next": "offer" },
    "again": { "speaker": "baker.name", "text": "baker.again", "next": "offer" },
    "offer": { "text": "baker.offer", "choices": [
      { "text": "baker.buy", "if": [{ "var": "gold", "at_least": 5 }],
        "effects": [{ "add": "gold", "by": -5 }, { "quest": "bread", "to": "done" }, { "event": "sold" }],
        "next": "thanks" },
      { "text": "baker.bye", "end": true } ] },
    "thanks": { "text": "baker.thanks", "end": true }
  }
})";

bool says(const std::vector<std::string>& lines, const std::string& part) {
  return std::any_of(lines.begin(), lines.end(),
                     [&](const std::string& line) { return line.find(part) != std::string::npos; });
}

// The errors of a dialogue whose only node is `node`.
std::vector<std::string> errors_of_node(const std::string& node) {
  return read_dialogue(R"({"start": "a", "nodes": {"a": )" + node + "}}").errors;
}

}  // namespace

TEST_CASE("a well made dialogue reads, with the texts and quests it names") {
  const DialogueFile file = read_dialogue(kBaker);
  CHECK(file.ok());
  CHECK(file.warnings.empty());
  CHECK(file.json["nodes"]["hello"]["voice"] == "baker/hello");
  const auto has_text = [&](const std::string& key) {
    return std::any_of(file.texts.begin(), file.texts.end(), [&](const NameUse& use) { return use.name == key; });
  };
  for (const char* key : {"baker.name", "baker.hello", "baker.offer", "baker.buy", "baker.bye", "baker.thanks"}) {
    CHECK_MESSAGE(has_text(key), key);
  }
  REQUIRE(file.quests.size() == 1);
  CHECK(file.quests[0].name == "bread");
  CHECK(file.quests[0].where == "node offer choice 1");
}

TEST_CASE("what is not a dialogue says so") {
  CHECK(says(read_dialogue("{ nope").errors, "not JSON"));
  CHECK(says(read_dialogue("[]").errors, "a JSON object"));
  CHECK(says(read_dialogue(R"({"start": "a"})").errors, "\"nodes\" must be an object"));
  CHECK(says(read_dialogue(R"({"nodes": {"a": {"text": "t", "end": true}}})").errors, "\"start\" must say"));
  const std::string deep = std::string(kMaxDepth + 1, '[') + std::string(kMaxDepth + 1, ']');
  CHECK(says(read_dialogue(deep).errors, "not JSON"));
}

TEST_CASE("a node that leads nowhere is a dead end") {
  CHECK(says(errors_of_node(R"({"text": "t"})"), "node a: it leads nowhere"));
  CHECK(says(errors_of_node(R"({"text": "t", "end": false})"), "\"end\" can only be true"));
  CHECK(says(errors_of_node(R"({"text": "t", "end": true, "next": "a"})"), "more than one way"));
  CHECK(says(errors_of_node(R"({"text": "t", "choices": [{"text": "c"}]})"), "node a choice 1: it leads nowhere"));
  CHECK(says(errors_of_node(R"({"text": "t", "choices": []})"), "at least one choice"));
}

TEST_CASE("every node named is there") {
  CHECK(says(read_dialogue(R"({"start": "b", "nodes": {"a": {"text": "t", "end": true}}})").errors,
             "start: there is no node \"b\""));
  CHECK(says(errors_of_node(R"({"text": "t", "next": [{"next": "nowhere"}]})"), "branch 1: there is no node"));
  CHECK(says(errors_of_node(R"({"text": "t", "choices": [{"text": "c", "next": "x"}]})"), "there is no node \"x\""));
}

TEST_CASE("a misspelt key is an error, not passed over") {
  CHECK(says(errors_of_node(R"({"txt": "t", "end": true})"), "\"txt\" is not something it can have"));
  CHECK(says(errors_of_node(R"({"text": "t", "end": true})"), "no such") == false);
  CHECK(says(errors_of_node(R"({"text": "t", "end": true, "effects": [{"set": "x", "too": 1}]})"),
             "\"too\" is not something"));
}

TEST_CASE("conditions read a variable or a quest, and hold against one value") {
  const auto with_condition = [](const std::string& condition) {
    return errors_of_node(R"({"text": "t", "choices": [{"text": "c", "end": true, "if": [)" + condition + "]}]}");
  };
  CHECK(with_condition(R"({"var": "gold", "at_most": 3})").empty());
  CHECK(with_condition(R"({"quest": "bread", "not": "done"})").empty());
  CHECK(says(with_condition(R"({"var": "gold"})"), "one of \"is\""));
  CHECK(says(with_condition(R"({"var": "gold", "quest": "q", "is": 1})"), "one \"var\" or one \"quest\""));
  CHECK(says(with_condition(R"({"var": "gold", "at_least": "five"})"), "\"at_least\" is a number"));
  CHECK(says(with_condition(R"({"var": "gold", "is": [1]})"), "null, true, false"));
  CHECK(says(with_condition(R"({"var": "", "is": 1})"), "a name of 1 to"));
}

TEST_CASE("effects are those a dialogue can have, with what each needs") {
  const auto with_effect = [](const std::string& effect) {
    return errors_of_node(R"({"text": "t", "end": true, "effects": [)" + effect + "]}");
  };
  CHECK(with_effect(R"({"event": "sold"})").empty());
  CHECK(with_effect(R"({"event": "sold", "data": {"price": 5}})").empty());
  CHECK(says(with_effect(R"({"set": "x"})"), "needs \"to\""));
  CHECK(says(with_effect(R"({"add": "x", "by": "1"})"), "\"by\" is a number"));
  CHECK(says(with_effect(R"({"quest": "q", "to": 2})"), "a stage"));
  CHECK(says(with_effect(R"({"give": "x"})"), "one of \"set\""));
  CHECK(says(with_effect(R"({"set": "x", "add": "y", "to": 1})"), "one of \"set\""));
}

TEST_CASE("a voice is a sound's name inside the game") {
  CHECK(errors_of_node(R"({"text": "t", "end": true, "voice": "baker/hi"})").empty());
  CHECK(says(errors_of_node(R"({"text": "t", "end": true, "voice": "../../secret"})"), "\"voice\" is a sound's"));
}

TEST_CASE("what may be a mistake is a warning") {
  const DialogueFile unreached =
      read_dialogue(R"({"start": "a", "nodes": {"a": {"text": "t", "end": true}, "b": {"text": "t", "end": true}}})");
  CHECK(unreached.ok());
  CHECK(says(unreached.warnings, "node b: nothing leads to it"));
  const DialogueFile conditional = read_dialogue(
      R"({"start": "a", "nodes": {"a": {"text": "t", "choices": [{"text": "c", "if": [{"var": "v", "is": 1}],
         "end": true}]}}})");
  CHECK(says(conditional.warnings, "every choice has a condition"));
}

TEST_CASE("a dialogue's name stays in the dialogues folder") {
  CHECK(is_dialogue_name("baker"));
  CHECK(is_dialogue_name("village/baker"));
  CHECK_FALSE(is_dialogue_name("../scripts/player"));
  CHECK_FALSE(is_dialogue_name("/etc/passwd"));
  CHECK_FALSE(is_dialogue_name(""));
  CHECK_FALSE(is_dialogue_name(std::string(kMaxNameBytes + 1, 'a')));
}

TEST_CASE("the stages of quests a dialogue names are listed for the check") {
  const DialogueFile file = read_dialogue(R"({"start": [{"if": [{"quest": "bread", "not": "done"}], "next": "a"},
    {"next": "a"}], "nodes": {"a": {"text": "t", "end": true, "effects": [{"quest": "bread", "to": "baked"}]}}})");
  REQUIRE(file.stages.size() == 2);
  CHECK(file.stages[0].quest == "bread");
  CHECK(file.stages[0].stage == "done");
  CHECK(file.stages[0].where == "start branch 1");
  CHECK(file.stages[1].stage == "baked");
  CHECK(file.stages[1].where == "node a");
}
