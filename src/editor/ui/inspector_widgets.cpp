// What the Inspector's cards share: the widgets they are made of, and how an edit reaches the document.

#include <imgui.h>
#include <imgui_stdlib.h>

#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/style.hpp"
#include "scene/props.hpp"
#include "script/script_naming.hpp"

namespace redgem::ui::inspector {
namespace {

// Where the parts of a vector and the channels of a color are in the arrays ImGui edits.
enum Axis { kX, kY, kZ };
enum Channel { kRed, kGreen, kBlue, kAlpha };

// What a field that was just left (Enter, Tab or a click elsewhere) has to apply; nullopt when
// it still has the focus, was never typed in, or was left as it was.
std::optional<TextEdit> edit_on_leaving(TextFieldState& field) {
  if (ImGui::IsItemActive()) {
    field.editing = true;
    return std::nullopt;
  }
  if (!field.editing) return std::nullopt;
  field.editing = false;
  if (field.buffer == field.original) return std::nullopt;
  return TextEdit{field.object, field.buffer};
}

}  // namespace

void streamed_edit(PanelContext& context) {
  context.state.inspector.edits.before_change(context.session.document(), context.session.selection());
}

void set_or_remove(EditorSession& session, int index, const char* key, const std::optional<PropertyValue>& value) {
  if (value) {
    session.document().set_prop(index, key, *value);
  } else {
    session.document().remove_prop(index, key);
  }
  session.refresh_preview();
}

std::vector<std::string> scripts_of(EditorSession& session, int index) {
  return split_script_list(get_string(session.document().node(index).props, props::kScript).value_or(""));
}

std::optional<TextEdit> text_field(PanelContext& context, const char* id, int object, const std::string& current,
                                   const char* hint) {
  TextFieldState& field = context.state.inspector.text_fields[ImGui::GetID(id)];
  if (!field.editing) field = {current, current, object, false};  // follow the document while idle
  ImGui::InputTextWithHint(id, hint, &field.buffer);
  const std::optional<TextEdit> edit = edit_on_leaving(field);
  if (!edit) return std::nullopt;
  return context.session.document().has_node(edit->object) ? edit : std::nullopt;
}

bool drag_vec3(const char* label, const char* id, Vec3& value, float speed, float lowest, float highest) {
  float numbers[] = {value.x, value.y, value.z};
  property_label(label);
  if (!ImGui::DragFloat3(id, numbers, speed, lowest, highest)) return false;
  value = {numbers[kX], numbers[kY], numbers[kZ]};
  return true;
}

bool edit_color(const char* id, Color& color, ImGuiColorEditFlags flags) {
  float channels[] = {color.r, color.g, color.b, color.a};
  if (!ImGui::ColorEdit4(id, channels, flags)) return false;
  color = {channels[kRed], channels[kGreen], channels[kBlue], channels[kAlpha]};
  return true;
}

bool edit_rgb(const char* id, Vec3& color, ImGuiColorEditFlags flags) {
  float channels[] = {color.x, color.y, color.z};
  if (!ImGui::ColorEdit3(id, channels, flags)) return false;
  color = {channels[kRed], channels[kGreen], channels[kBlue]};
  return true;
}

bool edit_rgb(const char* id, Color& color, ImGuiColorEditFlags flags) {
  Vec3 rgb{color.r, color.g, color.b};
  if (!edit_rgb(id, rgb, flags)) return false;
  color = {rgb.x, rgb.y, rgb.z, color.a};
  return true;
}

}  // namespace redgem::ui::inspector
