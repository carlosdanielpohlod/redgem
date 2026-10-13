// Inspector cards of an object's physics: its body, collider and mass, and a vehicle's wheels.

#include <algorithm>
#include <cfloat>
#include <iterator>

#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/inspector_logic.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/style.hpp"
#include "scene/props.hpp"

namespace redgem::ui::inspector {
namespace {

using namespace editor_config;
using namespace inspector_logic;

// What each of props::kBodyTypes is for, in their order.
constexpr const char* kBodyDescriptions[] = {"Never moves (floors, walls)", "Falls and gets pushed (crates, balls)",
                                             "Moved by scripts, pushes others", "Walks and jumps (CharacterController)",
                                             "Drives on its wheel children"};
constexpr int kDefaultBody = 0;      // the first of props::kBodyTypes
constexpr int kDefaultCollider = 0;  // the first of props::kColliders
constexpr const char* kTriggerId = "##trigger";
constexpr const char* kBodyId = "##body";
constexpr const char* kColliderId = "##collider";
constexpr const char* kMassId = "##mass";
constexpr const char* kWheelId = "##wheel";

// One undo step for the whole component.
void remove_physics(EditorSession& session, int index) {
  SceneDocument& doc = session.document();
  doc.checkpoint();
  for (const char* key : {props::kBody, props::kCollider, props::kMass, props::kTrigger}) {
    if (doc.node(index).props.count(key) != 0) doc.remove_prop(index, key, false);
  }
  doc.drop_checkpoint_if_unchanged();
  session.refresh_preview();
}

// Each of the rows below returns true when it changed the object: the card is then drawn again
// from the new properties on the next frame.
bool draw_trigger_row(EditorSession& session, int index, bool& trigger) {
  property_label("Trigger");
  if (ImGui::Checkbox(kTriggerId, &trigger)) {
    set_or_remove(session, index, props::kTrigger, flag_or_nothing(trigger, true));
    return true;
  }
  tooltip("Detects overlaps (on_trigger_enter) instead of colliding.");
  return false;
}

bool draw_body_row(EditorSession& session, int index, const PropertyMap& props) {
  const int count = static_cast<int>(std::size(props::kBodyTypes));
  const std::string current = get_string(props, props::kBody).value_or(props::kBodyTypes[kDefaultBody]);
  int type = editor_logic::index_of(props::kBodyTypes, count, current, kDefaultBody);
  property_label("Body");
  if (ImGui::Combo(kBodyId, &type, props::kBodyTypes, count)) {
    set_or_remove(session, index, props::kBody, PropertyValue(std::string(props::kBodyTypes[type])));
    return true;
  }
  tooltip(kBodyDescriptions[type]);
  return false;
}

bool draw_collider_row(EditorSession& session, int index, const PropertyMap& props) {
  const int count = static_cast<int>(std::size(props::kColliders));
  const std::string current = get_string(props, props::kCollider).value_or(props::kColliders[kDefaultCollider]);
  int collider = editor_logic::index_of(props::kColliders, count, current, kDefaultCollider);
  property_label("Collider");
  if (!ImGui::Combo(kColliderId, &collider, props::kColliders, count)) return false;
  set_or_remove(session, index, props::kCollider, PropertyValue(std::string(props::kColliders[collider])));
  return true;
}

void draw_mass_row(PanelContext& context, int index, float mass) {
  property_label("Mass");
  if (!ImGui::DragFloat(kMassId, &mass, kDragSpeedNumber, kMinMass, FLT_MAX, "%.2f kg")) return;
  streamed_edit(context);
  context.session.document().set_prop(index, props::kMass, static_cast<double>(std::max(mass, kMinMass)), false);
}

// The rows that say what the object is to the physics, until one of them changes it. (A trigger
// has no body to choose.)
bool draw_kind_rows(EditorSession& session, int index, const PropertyMap& props) {
  bool trigger = get_bool(props, props::kTrigger).value_or(false);
  if (draw_trigger_row(session, index, trigger)) return true;
  if (trigger) return draw_collider_row(session, index, props);
  return draw_body_row(session, index, props) || draw_collider_row(session, index, props);
}

void draw_physics_rows(PanelContext& context, int index, const BodyConfig& body) {
  if (draw_kind_rows(context.session, index, context.session.document().node(index).props)) return;
  if (has_mass(body)) draw_mass_row(context, index, body.mass);
  for (const std::string& warning : body.warnings) {
    ImGui::TextColored(warning_color(), ICON_FA_TRIANGLE_EXCLAMATION "  %s", warning.c_str());
  }
}

}  // namespace

void draw_physics(PanelContext& context, int index) {
  const PropertyMap& props = context.session.document().node(index).props;
  if (!has_physics(props)) return;
  const BodyConfig body = body_config_from_props(props);
  bool removed = false;
  const bool open = component_header(ICON_FA_WEIGHT_HANGING, "Physics", &removed);
  if (removed) {
    remove_physics(context.session, index);
  } else if (open) {
    draw_physics_rows(context, index, body);
  }
}

void draw_wheel(EditorSession& session, int index) {
  const NodeDesc& node = session.document().node(index);
  if (node.parent == kNoIndex) return;
  const auto parent_body = get_string(session.document().node(node.parent).props, props::kBody);
  const bool is_wheel = get_bool(node.props, props::kWheel).value_or(false);
  if (!wheel_card_shown(parent_body, is_wheel)) return;
  if (!component_header(ICON_FA_CAR, "Vehicle wheel")) return;
  bool wheel = is_wheel;
  property_label("Is a wheel");
  if (ImGui::Checkbox(kWheelId, &wheel)) set_or_remove(session, index, props::kWheel, flag_or_nothing(wheel, true));
}

void add_body_menu(EditorSession& session, int index) {
  for (int i = 0; i < static_cast<int>(std::size(props::kBodyTypes)); ++i) {
    if (ImGui::MenuItem(props::kBodyTypes[i])) {
      set_or_remove(session, index, props::kBody, PropertyValue(std::string(props::kBodyTypes[i])));
    }
    tooltip(kBodyDescriptions[i]);
  }
}

}  // namespace redgem::ui::inspector
