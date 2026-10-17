// ProjectHub: its two columns. On the left a new game is named and given a template; on the
// right are the games there are, and a field for the folder of any other.

#include "project/project_hub.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"
#include "project/game_templates.hpp"
#include "project/project_hub_card.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr float kTemplateCardHeight = 58.0f;  // pixels
constexpr float kOpenButtonWidth = 90.0f;     // pixels
constexpr float kOpenFooterRows = 2.0f;       // "Open a game folder" heading and its path field
constexpr int kButtonColors = 2;              // normal and hovered color of the Create button
constexpr const char* kGameListId = "list";
constexpr const char* kNameId = "##name";
constexpr const char* kPathId = "##path";

// The button that stands out: in the accent color.
bool accent_button(const char* label, float width) {
  ImGui::PushStyleColor(ImGuiCol_Button, ui::accent_color());
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ui::accent_color());
  const bool pressed = ImGui::Button(label, ImVec2(width, 0.0f));
  ImGui::PopStyleColor(kButtonColors);
  return pressed;
}

}  // namespace

void ProjectHub::draw_new_game(float width) {
  ImGui::SeparatorText(ui::icon_label(ICON_FA_FILE_CIRCLE_PLUS, "project.hub.new_game"));
  ImGui::TextColored(ui::muted_color(), "%s", tr("project.hub.name"));
  ImGui::SetNextItemWidth(kFillWidth);
  const bool entered = ImGui::InputText(kNameId, &new_name_, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::Spacing();
  draw_templates();
  ImGui::Spacing();
  ImGui::TextColored(ui::muted_color(), "%s", shown_path(options_.games_folder / new_name_).c_str());
  ImGui::Spacing();
  const bool asked = accent_button(ui::icon_label(ICON_FA_PLUS, "project.hub.create_game"), width) || entered;
  if (asked) create_game(new_name_, template_);
  draw_error();
}

// What a new game starts from: one card per template, the chosen one selected.
void ProjectHub::draw_templates() {
  ImGui::TextColored(ui::muted_color(), "%s", tr("project.hub.start_from"));
  for (const GameTemplate& known : game_templates()) {
    const std::string title = editor_logic::capitalized(known.name);
    const hub::CardLabel label{known.name.c_str(), hub::template_icon(known.icon), title, known.description};
    if (hub::card(label, template_ == known.name, kTemplateCardHeight)) template_ = known.name;
  }
}

void ProjectHub::draw_error() {
  if (error_.empty()) return;
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ui::error_color());
  ImGui::TextWrapped(ICON_FA_CIRCLE_XMARK "  %s", error_.c_str());
  ImGui::PopStyleColor();
}

void ProjectHub::draw_games() {
  ImGui::SeparatorText(ui::icon_label(ICON_FA_GAMEPAD, "project.hub.games"));
  const float footer = ImGui::GetFrameHeightWithSpacing() * kOpenFooterRows + ImGui::GetStyle().ItemSpacing.y;
  ImGui::BeginChild(kGameListId, ImVec2(0.0f, -footer));
  draw_game_list();
  ImGui::EndChild();
  draw_open_folder();
}

void ProjectHub::draw_game_list() {
  if (games_.empty()) ImGui::TextColored(ui::muted_color(), "%s", tr("project.hub.no_games"));
  for (std::size_t i = 0; i < games_.size(); ++i) {
    const std::string id = games_[i].string();
    const std::string path = shown_path(games_[i]);
    const bool clicked = hub::card({id.c_str(), ICON_FA_GAMEPAD, titles_[i], path}, false, kHubCardHeight);
    const bool opened = clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    if (opened) open_game(games_[i]);
    ui::tooltip(tr("project.hub.open_hint"));
  }
}

// A field for the path of a game that is not in the list.
void ProjectHub::draw_open_folder() {
  ImGui::SeparatorText(ui::icon_label(ICON_FA_FOLDER_OPEN, "project.hub.open_folder"));
  ImGui::SetNextItemWidth(-kOpenButtonWidth - ImGui::GetStyle().ItemSpacing.x);
  const bool entered =
      ImGui::InputTextWithHint(kPathId, tr("project.hub.path_hint"), &open_path_, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  const bool asked = ImGui::Button(tr("project.hub.open"), ImVec2(kOpenButtonWidth, 0.0f)) || entered;
  if (asked) open_game(open_path_);
}

}  // namespace redgem
