#include "project/project_hub_card.hpp"

#include <imgui.h>

#include <algorithm>
#include <iterator>

#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/ui/style.hpp"

namespace redgem::hub {
namespace {

using namespace editor_config;

constexpr float kCardTextLines = 2.0f;   // title and detail
constexpr float kCardIconColumn = 2.0f;  // icon column width, in icon buttons
constexpr const char* kCardId = "##card";

// The icons a template can ask for by name (its template.json); the first stands in for any other.
struct NamedIcon {
  const char* name;
  const char* icon;
};
constexpr NamedIcon kTemplateIcons[] = {{"cube", ICON_FA_CUBE}, {"person-running", ICON_FA_PERSON_RUNNING}};

// What a card shows, over a selectable that starts at `start`: the icon, and next to it the two
// lines (with the gap between them) centered in the card's height.
void draw_card_text(ImVec2 start, float height, const char* icon, const std::string& title, const std::string& detail) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImGuiStyle& style = ImGui::GetStyle();
  const float line = ImGui::GetTextLineHeight();
  const float text_top = start.y + editor_logic::centered_start(height - line * kCardTextLines, style.ItemSpacing.y);
  const float icon_width = ImGui::GetFontSize() * kIconButtonWidth * kCardIconColumn;
  draw->AddText(ImVec2(start.x + style.FramePadding.x, start.y + editor_logic::centered_start(height, line)),
                ImGui::GetColorU32(ui::accent_color()), icon);
  draw->AddText(ImVec2(start.x + icon_width, text_top), ImGui::GetColorU32(ImGuiCol_Text), title.c_str());
  draw->AddText(ImVec2(start.x + icon_width, text_top + line + style.ItemSpacing.y),
                ImGui::GetColorU32(ui::muted_color()), detail.c_str());
}

}  // namespace

const char* template_icon(const std::string& name) {
  const auto* found = std::find_if(std::begin(kTemplateIcons), std::end(kTemplateIcons),
                                   [&name](const NamedIcon& known) { return name == known.name; });
  return found != std::end(kTemplateIcons) ? found->icon : std::begin(kTemplateIcons)->icon;
}

bool card(const CardLabel& label, bool selected, float height) {
  ImGui::PushID(label.id);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::Selectable(kCardId, selected, ImGuiSelectableFlags_AllowDoubleClick,
                                         ImVec2(ImGui::GetContentRegionAvail().x, height));
  draw_card_text(start, height, label.icon, label.title, label.detail);
  ImGui::PopID();
  return clicked;
}

}  // namespace redgem::hub
