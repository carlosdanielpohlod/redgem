#pragma once

// The Inspector, card by card: each draws one component of the object at `index`, when the object
// has it. What the cards share to edit the document is at the end.

#include <imgui.h>

#include <optional>
#include <string>
#include <vector>

#include "editor/ui/panels.hpp"

namespace redgem::ui::inspector {

// The object's icon, name, tag and flags, and where it is.
void draw_header(PanelContext& context, int index);
void draw_transform(PanelContext& context, int index);
// The mesh with its materials; a light's and a camera's settings.
void draw_mesh(PanelContext& context, int index);
void draw_light(PanelContext& context, int index);
void draw_camera(PanelContext& context, int index);
// The brush that sculpts a terrain in the Scene view.
void draw_terrain(PanelContext& context, int index);
// One card per attached script.
void draw_scripts(PanelContext& context, int index);
void draw_physics(PanelContext& context, int index);
// Wheels are children of a vehicle: shown on the child itself.
void draw_wheel(EditorSession& session, int index);
// The properties without a card of their own.
void draw_custom_props(PanelContext& context, int index);

// What Add Component offers for each kind of component.
void add_script_menu(PanelContext& context, int index);
void add_body_menu(EditorSession& session, int index);
void add_property_menu(PanelContext& context, int index);

// Call before a held widget (a drag, a slider, a color picker) changes the selected object: the
// whole interaction is one undo step (see EditStream).
void streamed_edit(PanelContext& context);
// Sets a property, or removes it for nullopt, and shows the result.
void set_or_remove(EditorSession& session, int index, const char* key, const std::optional<PropertyValue>& value);
// The scripts attached to an object, in order.
std::vector<std::string> scripts_of(EditorSession& session, int index);

// A text edit to apply: to `object`, the one the field showed when typing started (the selection
// may have changed since).
struct TextEdit {
  int object = kNoIndex;
  std::string text;
};
// Commits a text field when the user leaves it, if the object it showed is still there. Each
// field keeps its own buffer, so moving from one field to another never loses typing; clicking
// another object commits to the one being edited.
std::optional<TextEdit> text_field(PanelContext& context, const char* id, int object, const std::string& current,
                                   const char* hint = "");

// Widgets: each returns true when it changed the value it shows.
// A labelled row of three numbers.
bool drag_vec3(const char* label, const char* id, Vec3& value, float speed, float lowest = 0.0f, float highest = 0.0f);
bool edit_color(const char* id, Color& color, ImGuiColorEditFlags flags);
// A color without transparency, as a vector or as a Color whose alpha stays as it is.
bool edit_rgb(const char* id, Vec3& color, ImGuiColorEditFlags flags);
bool edit_rgb(const char* id, Color& color, ImGuiColorEditFlags flags);
// A drop-down over the values of an enum, listed by `names` in the enum's order.
template <typename Enum, std::size_t Count>
bool choose(const char* id, Enum& value, const char* const (&names)[Count]) {
  int chosen = static_cast<int>(value);
  if (!ImGui::Combo(id, &chosen, names, static_cast<int>(Count))) return false;
  value = static_cast<Enum>(chosen);
  return true;
}

}  // namespace redgem::ui::inspector
