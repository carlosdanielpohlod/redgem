// Quest files looked over as the engine reads one and `redgem check` reads them all: what makes one
// unusable is an error, what may be a mistake a warning, and the texts, items and dialogues it names
// are listed for the check.

#include <doctest/doctest.h>

#include <algorithm>
#include <string>

#include "script/quest_file.hpp"

using namespace redgem;
using namespace redgem::quest;

namespace {

// Bread for the baker: bring flour from the mill, then talk to the baker, who bakes it.
constexpr const char* kBread = R"({
  "title": "bread.title",
  "description": "bread.description",
  "start": "flour",
  "stages": {
    "flour": { "text": "bread.flour", "objectives": [
      { "go_to": "Mill", "radius": 3, "text": "bread.go_to_mill" },
      { "collect": "flour", "count": 3, "text": "bread.collect" } ], "next": "bring" },
    "bring": { "text": "bread.bring", "objectives": [
      { "deliver": "flour", "to": "Baker", "count": 3, "text": "bread.deliver" },
      { "talk_to": "Baker", "dialogue": "baker", "node": "thanks", "text": "bread.talk" },
      { "defeat": "Wolf", "text": "bread.wolf" },
      { "check": "oven_hot", "text": "bread.oven" } ], "next": "done" },
    "waiting": { "text": "bread.waiting" },
    "done": { "text": "bread.done", "end": true }
  }
})";

bool says(const std::vector<std::string>& lines, const std::string& part) {
  return std::any_of(lines.begin(), lines.end(),
                     [&](const std::string& line) { return line.find(part) != std::string::npos; });
}

// The errors of a quest whose only stages are `stages` (and one that ends it).
std::vector<std::string> errors_of_stages(const std::string& stages) {
  return read_quest(R"({"title": "t", "start": "a", "stages": {"end": {"text": "t", "end": true}, )" + stages + "}}")
      .errors;
}

std::vector<std::string> errors_of_objective(const std::string& objective) {
  return errors_of_stages(R"("a": {"text": "t", "objectives": [)" + objective + R"(], "next": "end"})");
}

}  // namespace

TEST_CASE("a quest that is well made reads with no errors, and lists what it names") {
  const QuestFile file = read_quest(kBread);
  CHECK(file.ok());
  CHECK(file.warnings.empty());
  CHECK(file.stages == std::set<std::string>{"bring", "done", "flour", "waiting"});
  CHECK(file.texts.size() == 12);
  REQUIRE(file.items.size() == 2);
  CHECK(file.items[0].name == "flour");
  REQUIRE(file.dialogues.size() == 1);
  CHECK(file.dialogues[0].dialogue == "baker");
  CHECK(file.dialogues[0].node == "thanks");
  CHECK(file.dialogues[0].where == "stage bring objective 2");
}

TEST_CASE("a quest that is not JSON, or is not made of stages, says so") {
  CHECK(says(read_quest("{").errors, "not JSON"));
  CHECK(says(read_quest("[]").errors, "a JSON object"));
  CHECK(says(read_quest(R"({"title": "t", "start": "a"})").errors, "\"stages\" must be"));
  CHECK(says(read_quest(R"({"title": "t", "stages": {"a": {"text": "t", "end": true}}})").errors, "\"start\" must"));
  CHECK(says(read_quest(R"({"start": "a", "stages": {"a": {"text": "t", "end": true}}})").errors, "no \"title\""));
}

TEST_CASE("every stage named must be there") {
  CHECK(says(read_quest(R"({"title": "t", "start": "b", "stages": {"a": {"text": "t", "end": true}}})").errors,
             "start: there is no stage \"b\""));
  CHECK(says(errors_of_stages(R"("a": {"text": "t", "objectives": [{"check": "c", "text": "t"}], "next": "nope"})"),
             "stage a: there is no stage \"nope\""));
}

TEST_CASE("a stage goes on in one way that fits it") {
  CHECK(says(errors_of_stages(R"("a": {"text": "t", "objectives": [{"check": "c", "text": "t"}]})"), "leads nowhere"));
  CHECK(says(errors_of_stages(R"("a": {"text": "t", "next": "end"})"), "it has none"));
  CHECK(says(errors_of_stages(R"("a": {"text": "t", "end": true, "next": "end"})"), "no objectives and no"));
  CHECK(says(errors_of_stages(R"("a": {"text": "t", "end": 1})"), "can only be true"));
  CHECK(says(errors_of_stages(R"("a": {"txt": "t"})"), "\"txt\" is not something"));
  CHECK(errors_of_stages(R"("a": {"text": "t"})").empty());  // (it waits for a dialogue or a script)
}

TEST_CASE("an objective that could never be done is an error") {
  CHECK(says(errors_of_objective(R"({"collect": "flour", "count": 0, "text": "t"})"), "could never be done"));
  CHECK(says(errors_of_objective(R"({"collect": "flour", "count": 1.5, "text": "t"})"), "whole number"));
  CHECK(says(errors_of_objective(R"({"go_to": "Mill", "radius": 0, "text": "t"})"), "could never be reached"));
  CHECK(says(errors_of_objective(R"({"deliver": "flour", "text": "t"})"), "who it goes \"to\""));
  CHECK(says(errors_of_objective(R"({"talk_to": "Baker", "node": "a", "text": "t"})"), "of its \"dialogue\""));
  CHECK(says(errors_of_objective(R"({"go_to": "", "text": "t"})"), "is a name of 1 to"));
  CHECK(says(errors_of_objective(R"({"go_to": "Mill", "count": 2, "text": "t"})"), "\"count\" is not something"));
  CHECK(says(errors_of_objective(R"({"go_to": "Mill", "defeat": "Wolf", "text": "t"})"), "an objective is one of"));
  CHECK(says(errors_of_objective(R"({"go_to": "Mill"})"), "no \"text\""));
}

TEST_CASE("a stage has a few objectives") {
  std::string many;
  for (std::size_t index = 0; index <= kMaxObjectives; ++index) many += R"({"check": "c", "text": "t"},)";
  many.pop_back();
  CHECK(says(errors_of_objective(many), "a list of 1 to"));
}

TEST_CASE("a quest that never completes is a warning") {
  const QuestFile file = read_quest(R"({"title": "t", "start": "a", "stages": {"a": {"text": "t"}}})");
  CHECK(file.ok());
  CHECK(says(file.warnings, "never completes"));
}

TEST_CASE("a quest's name stays in its folder and fits its variables") {
  CHECK(is_quest_name("village/bread"));
  CHECK_FALSE(is_quest_name("../bread"));
  CHECK_FALSE(is_quest_name(std::string(kMaxNameBytes + 1, 'q')));
  CHECK(shown_file("bread") == "quests/bread.json");
}
