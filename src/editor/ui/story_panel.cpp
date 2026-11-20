// The Story panel: a dialogue stepped through and a quest moved on, without Play (see
// editor/story_preview.hpp), with the conditions and effects of each step and the scratch
// GameVariables they read and change.

#include <imgui.h>

#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/story_preview.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"
#include "script/dialogue_file.hpp"
#include "script/quest_file.hpp"

namespace redgem::ui {
namespace {

// What a step's conditions, effects and "next" are, as JSON, when it has none.
constexpr const char* kNothing = "null";
constexpr const char* kNoneListed = "[]";

// A list to pick a file's name from, read again from the game's folder as it opens.
void name_picker(const char* label, const char* id, std::vector<std::string>& names, std::string& picked,
                 std::vector<std::string> (*read)(const std::filesystem::path&), const std::filesystem::path& game) {
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * editor_config::kStoryPickerWidth);
  if (!ImGui::BeginCombo(id, picked.empty() ? label : picked.c_str())) return;
  names = read(game);
  for (const std::string& name : names) {
    if (ImGui::Selectable(name.c_str(), name == picked)) picked = name;
  }
  ImGui::EndCombo();
}

// What a step says besides its text: its conditions, its effects and where it leads.
void step_details(const char* key, const std::string& json) {
  if (json == kNothing || json == kNoneListed) return;
  ImGui::SameLine();
  ImGui::TextColored(muted_color(), "%s %s", tr(key), json.c_str());
}

void draw_choice(StoryPreview& preview, const PreviewChoice& choice) {
  ImGui::PushID(choice.index);
  ImGui::BeginDisabled(!choice.offered);
  if (ImGui::Button(choice.text.c_str())) preview.choose(choice.index);
  ImGui::EndDisabled();
  step_details("editor.story.if", choice.conditions);
  step_details("editor.story.effects", choice.effects);
  step_details("editor.story.next", choice.next);
  ImGui::PopID();
}

void draw_line_text(const PreviewLine& line) {
  ImGui::TextColored(muted_color(), "%s / %s", line.dialogue.c_str(), line.node.c_str());
  if (!line.speaker.empty()) ImGui::TextColored(accent_color(), "%s", line.speaker.c_str());
  ImGui::TextWrapped("%s", line.text.c_str());
  ImGui::TextColored(muted_color(), "%s", line.text_key.c_str());
  step_details("editor.story.effects", line.effects);
}

void draw_line(StoryPreview& preview, const PreviewLine& line) {
  if (line.over) {
    ImGui::TextColored(muted_color(), "%s", tr("editor.story.over"));
    return;
  }
  draw_line_text(line);
  for (const PreviewChoice& choice : line.choices) draw_choice(preview, choice);
  if (!line.choices.empty()) return;
  if (ImGui::Button(icon_label(ICON_FA_FORWARD, "editor.story.continue"))) preview.go_on();
  step_details("editor.story.next", line.next);
}

void draw_quest(StoryPreview& preview, const PreviewQuest& quest) {
  ImGui::PushID(quest.name.c_str());
  ImGui::TextColored(quest.complete ? success_color() : accent_color(), "%s: %s", quest.name.c_str(),
                     quest.stage.c_str());
  if (!quest.text.empty()) ImGui::TextWrapped("%s", quest.text.c_str());
  for (const PreviewObjective& objective : quest.objectives) {
    ImGui::Text("%s %s", objective.done ? ICON_FA_SQUARE_CHECK : ICON_FA_SQUARE, objective.text.c_str());
  }
  for (const std::string& stage : quest.stages) {
    if (ImGui::SmallButton(stage.c_str())) preview.set_stage(quest.name, stage);
    ImGui::SameLine();
  }
  ImGui::NewLine();
  ImGui::PopID();
}

void draw_variables(const PreviewState& state) {
  if (state.variables.empty()) ImGui::TextColored(muted_color(), "%s", tr("editor.story.no_variables"));
  for (const PreviewVariable& variable : state.variables) {
    ImGui::Text("%s = %s", variable.name.c_str(), variable.value.c_str());
  }
}

void draw_dialogue_picker(StoryPreview& preview, StoryState& story, const std::filesystem::path& game) {
  name_picker(tr("editor.story.pick_dialogue"), "##dialogue", story.dialogues, story.dialogue, dialogue::dialogue_names,
              game);
  ImGui::SameLine();
  ImGui::BeginDisabled(story.dialogue.empty());
  if (ImGui::Button(icon_label(ICON_FA_PLAY, "editor.story.start_dialogue"))) preview.start_dialogue(story.dialogue);
  ImGui::EndDisabled();
}

void draw_quest_picker(StoryPreview& preview, StoryState& story, const std::filesystem::path& game) {
  name_picker(tr("editor.story.pick_quest"), "##quest", story.quests, story.quest, quest::quest_names, game);
  ImGui::SameLine();
  ImGui::BeginDisabled(story.quest.empty());
  if (ImGui::Button(icon_label(ICON_FA_FLAG, "editor.story.start_quest"))) preview.start_quest(story.quest);
  ImGui::EndDisabled();
}

void draw_pickers(StoryPreview& preview, StoryState& story, const std::filesystem::path& game) {
  draw_dialogue_picker(preview, story, game);
  ImGui::SameLine();
  draw_quest_picker(preview, story, game);
  ImGui::SameLine();
  if (ImGui::Button(icon_label(ICON_FA_ROTATE_LEFT, "editor.story.reset"))) preview.reset();
}

// (A copy: a step taken by a button changes the preview's state while the rest is drawn.)
void draw_runs(StoryPreview& preview) {
  const PreviewState state = preview.state();
  if (ImGui::CollapsingHeader(tr("editor.story.conversation"), ImGuiTreeNodeFlags_DefaultOpen)) {
    if (state.line) draw_line(preview, *state.line);
  }
  if (ImGui::CollapsingHeader(tr("editor.story.quests"), ImGuiTreeNodeFlags_DefaultOpen)) {
    for (const PreviewQuest& quest : state.quests) draw_quest(preview, quest);
  }
  if (ImGui::CollapsingHeader(tr("editor.story.variables"), ImGuiTreeNodeFlags_DefaultOpen)) draw_variables(state);
}

}  // namespace

void draw_story(StoryPreview& preview, StoryState& story, const std::filesystem::path& game_dir) {
  ImGui::Begin(window_title(editor_config::kStoryTitle, editor_config::kStoryWindow));
  draw_pickers(preview, story, game_dir);
  if (!preview.error().empty()) ImGui::TextColored(error_color(), "%s", preview.error().c_str());
  ImGui::Separator();
  draw_runs(preview);
  ImGui::End();
}

}  // namespace redgem::ui
