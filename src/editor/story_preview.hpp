#pragma once

// The Story panel's runs: a dialogue stepped through line by line, choice by choice, and a quest
// moved from stage to stage, without Play. They run in an engine of their own, with no scene, on
// the game's files on disk (ruby/engine/editor_preview.rb): its GameVariables are scratch ones that
// only the preview's conditions read and its effects change.

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace redgem {

class Engine;

// One answer of the line, whether its conditions hold now or not.
struct PreviewChoice {
  int index = 0;
  std::string text;
  std::string conditions;  // its "if", as JSON ("[]" when it has none)
  std::string effects;     // its "effects", as JSON
  std::string next;        // where it leads, as JSON (null: the end)
  bool offered = false;    // its conditions hold
};

// Where a conversation is.
struct PreviewLine {
  std::string dialogue;
  std::string node;  // empty once it is over
  bool over = false;
  std::string speaker;  // in the language in use; empty when the line has none
  std::string text;
  std::string text_key;
  std::string effects;  // the node's, as JSON
  std::string next;     // the node's "next", as JSON (null when it has none)
  std::vector<PreviewChoice> choices;
};

struct PreviewObjective {
  std::string text;
  bool done = false;
};

// A quest that started, where it is.
struct PreviewQuest {
  std::string name;
  std::string stage;
  bool complete = false;
  std::string text;  // the stage's
  std::string next;  // the stage it goes to once its objectives are done; empty when none
  std::vector<std::string> stages;
  std::vector<PreviewObjective> objectives;
};

struct PreviewVariable {
  std::string name;
  std::string value;  // as JSON
};

struct PreviewState {
  std::optional<PreviewLine> line;
  std::vector<PreviewQuest> quests;
  std::vector<PreviewVariable> variables;  // by name
};

// The state as ruby/engine/editor_preview.rb tells it.
PreviewState preview_state_from(const nlohmann::json& json);

class StoryPreview {
 public:
  explicit StoryPreview(std::filesystem::path game_dir);
  ~StoryPreview();
  StoryPreview(const StoryPreview&) = delete;
  StoryPreview& operator=(const StoryPreview&) = delete;

  // Each returns false, with error() saying why, when the files or the step went wrong (the state
  // is then as it was). A dialogue or a quest is read again from its file each time it starts.
  bool start_dialogue(const std::string& name);
  bool choose(int index);
  bool go_on();  // a line with no choices: on to what follows
  bool start_quest(const std::string& name);
  bool set_stage(const std::string& quest, const std::string& stage);
  // Every variable back to nothing, the conversation and the quests gone; the game's texts are
  // read again as the next run starts.
  void reset();

  const PreviewState& state() const { return state_; }
  const std::string& error() const { return error_; }

 private:
  bool ready();
  // Calls a method of EditorPreview with texts, and takes in the state it answers.
  bool ask(const std::string& method, const std::vector<std::string>& args);

  std::filesystem::path game_dir_;
  std::unique_ptr<Engine> engine_;
  PreviewState state_;
  std::string error_;
};

}  // namespace redgem
