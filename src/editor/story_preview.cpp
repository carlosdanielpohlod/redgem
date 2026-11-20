#include "editor/story_preview.hpp"

#include <mruby.h>
#include <mruby/string.h>

#include <optional>
#include <utility>

#include "app/engine.hpp"
#include "core/translations.hpp"

namespace redgem {
namespace {

constexpr const char* kPreviewModule = "EditorPreview";
constexpr const char* kPreviewScene = "story_preview";  // a scene no game has: no scene script runs
constexpr const char* kWhere = "story_preview.rb";

std::string text_of(const nlohmann::json& json, const char* key) {
  const auto found = json.find(key);
  return found != json.end() && found->is_string() ? found->get<std::string>() : std::string();
}

bool flag_of(const nlohmann::json& json, const char* key) {
  const auto found = json.find(key);
  return found != json.end() && found->is_boolean() && found->get<bool>();
}

// A part of the state shown as JSON (a condition, an effect, a "next"): null when it is not there.
std::string json_of(const nlohmann::json& json, const char* key) {
  const auto found = json.find(key);
  return found != json.end() ? found->dump() : nlohmann::json().dump();
}

const nlohmann::json& list_of(const nlohmann::json& json, const char* key) {
  static const nlohmann::json kNone = nlohmann::json::array();
  const auto found = json.find(key);
  return found != json.end() && found->is_array() ? *found : kNone;
}

PreviewChoice choice_from(const nlohmann::json& json) {
  PreviewChoice choice;
  choice.index = json.value("index", 0);
  choice.text = text_of(json, "text");
  choice.conditions = json_of(json, "if");
  choice.effects = json_of(json, "effects");
  choice.next = json_of(json, "next");
  choice.offered = flag_of(json, "offered");
  return choice;
}

PreviewLine line_from(const nlohmann::json& json) {
  PreviewLine line;
  line.dialogue = text_of(json, "name");
  line.node = text_of(json, "node");
  line.over = flag_of(json, "over");
  line.speaker = text_of(json, "speaker");
  line.text = text_of(json, "text");
  line.text_key = text_of(json, "text_key");
  line.effects = json_of(json, "effects");
  line.next = json_of(json, "next");
  for (const auto& choice : list_of(json, "choices")) line.choices.push_back(choice_from(choice));
  return line;
}

PreviewQuest quest_from(const nlohmann::json& json) {
  PreviewQuest quest{text_of(json, "name"),
                     text_of(json, "stage"),
                     flag_of(json, "complete"),
                     text_of(json, "text"),
                     text_of(json, "next"),
                     {},
                     {}};
  for (const auto& stage : list_of(json, "stages")) {
    if (stage.is_string()) quest.stages.push_back(stage.get<std::string>());
  }
  for (const auto& objective : list_of(json, "objectives")) {
    quest.objectives.push_back({text_of(objective, "text"), flag_of(objective, "done")});
  }
  return quest;
}

std::vector<PreviewVariable> variables_from(const nlohmann::json& json) {
  std::vector<PreviewVariable> variables;
  const auto found = json.find("variables");
  if (found == json.end() || !found->is_object()) return variables;
  for (const auto& [name, value] : found->items()) variables.push_back({name, value.dump()});
  return variables;
}

// What a method of EditorPreview answers (the state, as JSON text), or nothing with `error` saying why.
std::optional<std::string> answer_of(RubyVm& vm, const std::string& method, const std::vector<std::string>& args,
                                     std::string& error) {
  const int arena = mrb_gc_arena_save(vm.state());
  const RubyResult module = vm.eval(kPreviewModule, kWhere);
  const RubyResult answer = vm.call_with(module.value, method.c_str(), [&args](mrb_state* mrb) {
    std::vector<mrb_value> values;
    for (const std::string& arg : args) values.push_back(mrb_str_new(mrb, arg.data(), arg.size()));
    return values;
  });
  const bool told = answer.ok && mrb_string_p(answer.value);
  std::optional<std::string> text;
  if (told) text = std::string(RSTRING_PTR(answer.value), static_cast<std::size_t>(RSTRING_LEN(answer.value)));
  mrb_gc_arena_restore(vm.state(), arena);
  if (!told) error = answer.ok ? tr("editor.story.no_answer") : answer.error;
  return text;
}

}  // namespace

PreviewState preview_state_from(const nlohmann::json& json) {
  PreviewState state;
  if (!json.is_object()) return state;
  const auto line = json.find("dialogue");
  if (line != json.end() && line->is_object()) state.line = line_from(*line);
  for (const auto& quest : list_of(json, "quests")) state.quests.push_back(quest_from(quest));
  state.variables = variables_from(json);
  return state;
}

StoryPreview::StoryPreview(std::filesystem::path game_dir) : game_dir_(std::move(game_dir)) {}

StoryPreview::~StoryPreview() = default;

// The engine the runs go in, made as the first one starts.
bool StoryPreview::ready() {
  if (engine_) return true;
  EngineOptions options;
  options.game_dir = game_dir_;
  options.scene_name = kPreviewScene;
  options.fixed_step = true;
  engine_ = Engine::create(options, SceneDesc{});
  if (engine_) return true;
  error_ = tr("editor.story.no_engine");
  return false;
}

bool StoryPreview::ask(const std::string& method, const std::vector<std::string>& args) {
  if (!ready()) return false;
  const std::optional<std::string> answer = answer_of(engine_->scripts().vm(), method, args, error_);
  if (!answer) return false;
  state_ = preview_state_from(nlohmann::json::parse(*answer, nullptr, false));
  error_.clear();
  return true;
}

bool StoryPreview::start_dialogue(const std::string& name) { return ask("__dialogue", {name}); }

bool StoryPreview::choose(int index) { return ask("__choose", {std::to_string(index)}); }

bool StoryPreview::go_on() { return ask("__continue", {}); }

bool StoryPreview::start_quest(const std::string& name) { return ask("__start_quest", {name}); }

bool StoryPreview::set_stage(const std::string& quest, const std::string& stage) {
  return ask("__set_stage", {quest, stage});
}

void StoryPreview::reset() {
  engine_.reset();
  state_ = PreviewState{};
  error_.clear();
}

}  // namespace redgem
