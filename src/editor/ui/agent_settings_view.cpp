// Part of the Agent panel: the list of agents, edited in a popup and kept in agents.json.

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cstddef>

#include "core/translations.hpp"
#include "editor/ui/agent_panel.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/palette.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

constexpr const char* kSettingsPopup = "###agent_settings";
constexpr float kListWidth = 140.0f;   // pixels of the list of agents
constexpr float kFormWidth = 380.0f;   // pixels of the fields beside it
constexpr int kFieldLines = 4;         // lines of the arguments' and the environment's boxes

float field_height() { return ImGui::GetTextLineHeight() * kFieldLines + ImGui::GetStyle().FramePadding.y * 2.0f; }

// The text fields show the selected agent's arguments and variables a line each.
void load_fields(AgentSettings& settings) {
  if (settings.agents.empty()) return;
  const agent::AgentProfile& agent = settings.agents[settings.selected];
  settings.arguments = agent::lines_text(agent.arguments);
  settings.environment = agent::environment_text(agent.environment);
}

void store_fields(AgentSettings& settings) {
  if (settings.agents.empty()) return;
  agent::AgentProfile& agent = settings.agents[settings.selected];
  agent.arguments = agent::text_lines(settings.arguments);
  agent.environment = agent::environment_of_text(settings.environment);
}

void select(AgentSettings& settings, std::size_t index) {
  store_fields(settings);
  settings.selected = index;
  load_fields(settings);
}

void draw_list(AgentSettings& settings) {
  ImGui::BeginChild("agents", ImVec2(kListWidth, field_height() * 2.0f), ImGuiChildFlags_Borders);
  for (std::size_t index = 0; index < settings.agents.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));
    if (ImGui::Selectable(settings.agents[index].name.c_str(), index == settings.selected)) select(settings, index);
    ImGui::PopID();
  }
  ImGui::EndChild();
}

void add_agent(AgentSettings& settings) {
  store_fields(settings);
  settings.agents.push_back({tr("editor.agent.new_agent"), "", {}, {}, ""});
  settings.selected = settings.agents.size() - 1;
  load_fields(settings);
}

void remove_agent(AgentSettings& settings) {
  if (settings.agents.empty()) return;
  settings.agents.erase(settings.agents.begin() + static_cast<std::ptrdiff_t>(settings.selected));
  settings.selected = 0;
  load_fields(settings);
}

void restore_presets(AgentSettings& settings) {
  settings.agents = agent::preset_agents();
  settings.selected = 0;
  load_fields(settings);
}

void draw_list_buttons(AgentSettings& settings) {
  if (icon_button(ICON_FA_PLUS, tr("editor.agent.add_agent"))) add_agent(settings);
  ImGui::SameLine();
  if (icon_button(ICON_FA_TRASH, tr("editor.agent.remove_agent"))) remove_agent(settings);
  ImGui::SameLine();
  if (icon_button(ICON_FA_ARROWS_ROTATE, tr("editor.agent.restore_presets"))) restore_presets(settings);
}

void draw_form(AgentSettings& settings) {
  if (settings.agents.empty()) return;
  agent::AgentProfile& agent = settings.agents[settings.selected];
  ImGui::BeginGroup();
  ImGui::PushItemWidth(kFormWidth);
  ImGui::InputText(label_with_id("editor.agent.field_name", "name"), &agent.name);
  ImGui::InputText(label_with_id("editor.agent.field_command", "command"), &agent.command);
  ImGui::TextColored(palette::kMuted, "%s", tr("editor.agent.field_arguments"));
  ImGui::InputTextMultiline("##arguments", &settings.arguments, ImVec2(kFormWidth, field_height()));
  ImGui::TextColored(palette::kMuted, "%s", tr("editor.agent.field_environment"));
  ImGui::InputTextMultiline("##environment", &settings.environment, ImVec2(kFormWidth, field_height()));
  ImGui::PopItemWidth();
  ImGui::EndGroup();
}

}  // namespace

void AgentPanel::open_settings() {
  settings_ = {agents_, picked_, "", "", ""};
  load_fields(settings_);
  settings_open_ = true;
}

void AgentPanel::keep_settings() {
  store_fields(settings_);
  if (!agent::write_agents(setup_.agents_file, settings_.agents, settings_.error)) return;
  agents_ = settings_.agents.empty() ? agent::preset_agents() : settings_.agents;
  picked_ = std::min(picked_, agents_.size() - 1);
  settings_open_ = false;
  end_session();
}

void AgentPanel::draw_settings() {
  if (settings_open_) ImGui::OpenPopup(kSettingsPopup);
  const char* title = window_title("editor.agent.settings_title", kSettingsPopup);
  if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  draw_list(settings_);
  ImGui::SameLine();
  draw_form(settings_);
  draw_list_buttons(settings_);
  if (!settings_.error.empty()) ImGui::TextColored(palette::kError, "%s", settings_.error.c_str());
  ImGui::TextColored(palette::kMuted, "%s", tr("editor.agent.settings_note"));
  if (ImGui::Button(save_label())) keep_settings();
  ImGui::SameLine();
  if (ImGui::Button(cancel_label())) settings_open_ = false;
  if (!settings_open_) ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

}  // namespace redgem::ui
