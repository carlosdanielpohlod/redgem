#include <imgui.h>

#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/inspector_logic.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"
#include "scene/props.hpp"

namespace redgem::ui {
namespace {

using namespace editor_config;
using namespace inspector;

constexpr float kAddComponentWidth = 0.7f;  // of the panel width
constexpr const char* kAddComponentPopup = "add_component";

// What the Add Component button offers.
void draw_component_choices(PanelContext& context, int index) {
  EditorSession& session = context.session;
  menu(ICON_FA_GEM "  Script", [&context, index] { add_script_menu(context, index); });
  const bool no_physics = !inspector_logic::has_physics(session.document().node(index).props);
  menu(ICON_FA_WEIGHT_HANGING "  Physics body", [&session, index] { add_body_menu(session, index); }, no_physics);
  if (ImGui::MenuItem(ICON_FA_BULLSEYE "  Trigger", nullptr, false, no_physics)) {
    set_or_remove(session, index, props::kTrigger, PropertyValue(true));
  }
  menu(ICON_FA_SLIDERS "  Property", [&context, index] { add_property_menu(context, index); });
}

void draw_add_component(PanelContext& context, int index) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  const float available = ImGui::GetContentRegionAvail().x;
  const float width = available * kAddComponentWidth;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + editor_logic::centered_start(available, width));
  if (ImGui::Button(ICON_FA_PLUS "  Add Component", ImVec2(width, 0.0f))) ImGui::OpenPopup(kAddComponentPopup);
  popup(kAddComponentPopup, [&context, index] { draw_component_choices(context, index); });
}

// Where the running game has the object now.
void draw_live_transform(const Transform& world) {
  if (!component_header(ICON_FA_UP_DOWN_LEFT_RIGHT, "Transform (live)")) return;
  const Vec3 euler = quat_to_euler_degrees(world.rotation);
  property_label("Position");
  ImGui::Text("%.2f  %.2f  %.2f", world.position.x, world.position.y, world.position.z);
  property_label("Rotation");
  ImGui::Text("%.1f  %.1f  %.1f", euler.x, euler.y, euler.z);
}

void draw_runtime_view(EditorSession& session, int index) {
  const Scene& scene = session.scene();
  if (!scene.is_alive(index)) {
    ImGui::TextColored(muted_color(), "Destroyed during play.");
    return;
  }
  draw_live_transform(scene.world_transform(index));
  ImGui::Spacing();
  ImGui::TextColored(muted_color(), ICON_FA_CIRCLE_INFO "  Stop the game to edit.");
}

// The selection as cards, one per component it has.
void draw_components(PanelContext& context, int index) {
  draw_header(context, index);
  draw_transform(context, index);
  draw_mesh(context, index);
  draw_light(context, index);
  draw_camera(context, index);
  draw_terrain(context, index);
  draw_scripts(context, index);
  draw_physics(context, index);
  draw_wheel(context.session, index);
  draw_custom_props(context, index);
  draw_add_component(context, index);
}

// Widget IDs do not include the object: a text field being edited when another object gets
// selected still commits to the object it was showing (see inspector::text_field).
void draw_selection(PanelContext& context, int index) {
  EditorSession& session = context.session;
  if (session.editing()) {
    draw_components(context, index);
  } else {
    ImGui::TextUnformatted(session.document().node(index).name.c_str());
    draw_runtime_view(session, index);
  }
}

}  // namespace

void open_script(ScriptWorkspace& scripts, const std::string& file_name) {
  std::string error;
  if (scripts.open(file_name, error) == nullptr) log::error(error);
}

void draw_inspector(PanelContext& context) {
  ImGui::Begin(kInspectorWindow);
  const ObjectId index = context.session.selection();
  if (index == kNoObject) {
    ImGui::TextColored(muted_color(), ICON_FA_ARROW_POINTER "  Select an object in the Hierarchy or the Scene.");
  } else {
    draw_selection(context, index);
  }
  ImGui::End();
}

}  // namespace redgem::ui
