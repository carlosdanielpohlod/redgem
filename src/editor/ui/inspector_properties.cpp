// Inspector card of an object's custom properties, and the menu that adds one.

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <string>

#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/inspector_logic.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::inspector {
namespace {

using namespace editor_config;
using namespace inspector_logic;

// The keys of the names of the kinds of property, in PropertyKind's order.
constexpr const char* kPropertyKinds[] = {"editor.inspector.property.number", "editor.inspector.property.text",
                                          "editor.inspector.property.toggle"};
constexpr float kPopupFieldWidth = 160.0f;  // pixels: fields inside the menu
constexpr const char* kValueId = "##value";
constexpr const char* kNewPropertyId = "##key";
constexpr const char* kKindId = "##type";

// The button that adds a property: its icon, then its text (made once: see editor/ui/labels.hpp).
const char* add_property_label() {
  static const std::string label = std::string(ICON_FA_PLUS " ") + tr("editor.inspector.property.add");
  return label.c_str();
}

void draw_flag_value(EditorSession& session, int index, const std::string& key, bool flag) {
  if (ImGui::Checkbox(kValueId, &flag)) set_or_remove(session, index, key.c_str(), PropertyValue(flag));
}

void draw_number_value(PanelContext& context, int index, const std::string& key, double number) {
  if (!ImGui::DragScalar(kValueId, ImGuiDataType_Double, &number, kDragSpeedNumber)) return;
  streamed_edit(context);
  context.session.document().set_prop(index, key, number, false);
}

void draw_text_value(PanelContext& context, int index, const std::string& key, const std::string& current) {
  const auto text = text_field(context, kValueId, index, current);
  if (text) set_or_remove(context.session, text->object, key.c_str(), PropertyValue(text->text));
}

// The widget of one custom property, by what it holds: a checkbox, a number or a text field.
void draw_custom_value(PanelContext& context, int index, const std::string& key, const PropertyValue& value) {
  if (const auto* flag = std::get_if<bool>(&value)) {
    draw_flag_value(context.session, index, key, *flag);
  } else if (const auto* number = std::get_if<double>(&value)) {
    draw_number_value(context, index, key, *number);
  } else {
    draw_text_value(context, index, key, std::get<std::string>(value));
  }
}

// One custom property: a button that removes it, its name and its value.
void draw_custom_row(PanelContext& context, int index, const std::string& key, const PropertyValue& value) {
  if (ImGui::SmallButton(ICON_FA_XMARK)) {
    set_or_remove(context.session, index, key.c_str(), std::nullopt);
    return;
  }
  tooltip(tr("editor.inspector.property.remove"));
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(muted_color(), "%s", key.c_str());
  ImGui::SameLine(kLabelColumnWidth);
  ImGui::SetNextItemWidth(kFillWidth);
  draw_custom_value(context, index, key, value);
}

bool has_custom(const PropertyMap& props) {
  return std::any_of(props.begin(), props.end(), [](const auto& entry) { return is_custom(entry.first); });
}

}  // namespace

void draw_custom_props(PanelContext& context, int index) {
  const PropertyMap props = context.session.document().node(index).props;  // copy: edits change the map
  if (!has_custom(props)) return;
  if (!component_header(ICON_FA_SLIDERS, tr("editor.inspector.property.title"))) return;
  for (const auto& [key, value] : props) {
    if (!is_custom(key)) continue;
    ImGui::PushID(key.c_str());
    draw_custom_row(context, index, key, value);
    ImGui::PopID();
  }
  ImGui::TextColored(muted_color(), "%s", tr("editor.inspector.property.ruby_hint"));
}

void add_property_menu(PanelContext& context, int index) {
  InspectorState& state = context.state.inspector;
  std::string& key = state.new_property_key;
  ImGui::SetNextItemWidth(kPopupFieldWidth);
  ImGui::InputTextWithHint(kNewPropertyId, tr("editor.inspector.property.name_hint"), &key);
  ImGui::SetNextItemWidth(kPopupFieldWidth);
  choose(kKindId, state.new_property_type, kPropertyKinds);
  ImGui::BeginDisabled(!can_add_property(context.session.document().node(index).props, key));
  if (ImGui::Button(add_property_label())) {
    const auto kind = static_cast<PropertyKind>(state.new_property_type);
    set_or_remove(context.session, index, key.c_str(), initial_value(kind));
    key.clear();
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
}

}  // namespace redgem::ui::inspector
