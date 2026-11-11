// Inspector cards of an object's physics: its body, collider and mass, and a vehicle's wheels.

#include <algorithm>
#include <cfloat>
#include <iterator>

#include "core/translations.hpp"
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

// The keys of what each of props::kBodyTypes is for, in their order.
constexpr const char* kBodyDescriptions[] = {
    "editor.inspector.physics.static_body", "editor.inspector.physics.dynamic_body",
    "editor.inspector.physics.kinematic_body", "editor.inspector.physics.character_body",
    "editor.inspector.physics.vehicle_body"};
constexpr int kDefaultBody = 0;      // the first of props::kBodyTypes
constexpr int kDefaultCollider = 0;  // the first of props::kColliders
constexpr const char* kTriggerId = "##trigger";
constexpr const char* kBodyId = "##body";
constexpr const char* kColliderId = "##collider";
constexpr const char* kLayerId = "##layer";
constexpr const char* kMassId = "##mass";
constexpr const char* kWheelId = "##wheel";

// One undo step for the whole component.
void remove_physics(EditorSession& session, int index) {
  SceneDocument& doc = session.document();
  doc.checkpoint();
  for (const char* key : {props::kBody, props::kCollider, props::kMass, props::kTrigger, props::kLayer}) {
    if (doc.node(index).props.count(key) != 0) doc.remove_prop(index, key, false);
  }
  doc.drop_checkpoint_if_unchanged();
  session.refresh_preview();
}

// Each of the rows below returns true when it changed the object: the card is then drawn again
// from the new properties on the next frame.
bool draw_trigger_row(EditorSession& session, int index, bool& trigger) {
  property_label(tr("editor.inspector.physics.trigger"));
  if (ImGui::Checkbox(kTriggerId, &trigger)) {
    set_or_remove(session, index, props::kTrigger, flag_or_nothing(trigger, true));
    return true;
  }
  tooltip(tr("editor.inspector.physics.trigger_tooltip"));
  return false;
}

bool draw_body_row(EditorSession& session, int index, const PropertyMap& props) {
  const int count = static_cast<int>(std::size(props::kBodyTypes));
  const std::string current = get_string(props, props::kBody).value_or(props::kBodyTypes[kDefaultBody]);
  int type = editor_logic::index_of(props::kBodyTypes, count, current, kDefaultBody);
  property_label(tr("editor.inspector.physics.body"));
  if (ImGui::Combo(kBodyId, &type, props::kBodyTypes, count)) {
    set_or_remove(session, index, props::kBody, PropertyValue(std::string(props::kBodyTypes[type])));
    return true;
  }
  tooltip(tr(kBodyDescriptions[type]));
  return false;
}

bool draw_collider_row(EditorSession& session, int index, const PropertyMap& props) {
  const int count = static_cast<int>(std::size(props::kColliders));
  const std::string current = get_string(props, props::kCollider).value_or(props::kColliders[kDefaultCollider]);
  int collider = editor_logic::index_of(props::kColliders, count, current, kDefaultCollider);
  property_label(tr("editor.inspector.physics.collider"));
  if (!ImGui::Combo(kColliderId, &collider, props::kColliders, count)) return false;
  set_or_remove(session, index, props::kCollider, PropertyValue(std::string(props::kColliders[collider])));
  return true;
}

std::string layer_preview(const PropertyMap& props) {
  if (props.count(props::kLayer) == 0) return kDefaultLayerName;
  const auto layer = get_string(props, props::kLayer).value_or("");
  return layer.empty() ? tr("editor.inspector.physics.layer_invalid") : layer;
}

void warn_about_layer(BodyConfig& body, const PropertyMap& props, const CollisionLayers& layers) {
  if (props.count(props::kLayer) == 0) return;
  const auto layer = get_string(props, props::kLayer).value_or("");
  if (layer.empty()) {
    body.warnings.emplace_back(tr("editor.inspector.physics.layer_invalid_warning"));
  } else if (!layers.find(layer)) {
    body.warnings.emplace_back(tr("editor.inspector.physics.layer_unknown"));
  }
}

const std::string* pick_layer(const CollisionLayers& layers, const std::string& preview) {
  property_label(tr("editor.inspector.physics.layer"));
  if (!ImGui::BeginCombo(kLayerId, preview.c_str())) return nullptr;
  const std::string* picked = nullptr;
  for (std::size_t layer = 0; layer < layers.count(); ++layer) {
    const auto& name = layers.name(layer);
    if (ImGui::Selectable(name.c_str(), name == preview)) picked = &name;
  }
  ImGui::EndCombo();
  return picked;
}

bool draw_layer_row(EditorSession& session, int index) {
  const auto& props = session.document().node(index).props;
  const auto* picked = pick_layer(session.collision_layers(), layer_preview(props));
  return picked && session.set_collision_layer({index}, *picked);
}

void draw_mass_row(PanelContext& context, int index, float mass) {
  property_label(tr("editor.inspector.physics.mass"));
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
  const bool active = body.wants_physics();
  if (!active) ImGui::TextWrapped("%s", tr("editor.inspector.physics.layer_no_body"));
  const auto& props = context.session.document().node(index).props;
  const bool changed_kind = active && draw_kind_rows(context.session, index, props);
  if (changed_kind) return;
  if (draw_layer_row(context.session, index)) return;
  if (has_mass(body)) draw_mass_row(context, index, body.mass);
  ImGui::PushStyleColor(ImGuiCol_Text, warning_color());
  for (const std::string& warning : body.warnings) {
    ImGui::TextWrapped(ICON_FA_TRIANGLE_EXCLAMATION "  %s", warning.c_str());
  }
  ImGui::PopStyleColor();
}

}  // namespace

void draw_physics(PanelContext& context, int index) {
  const PropertyMap& props = context.session.document().node(index).props;
  if (!has_physics(props)) return;
  BodyConfig body = body_config_from_props(props);
  warn_about_layer(body, props, context.session.collision_layers());
  bool removed = false;
  const bool open = component_header(ICON_FA_WEIGHT_HANGING, tr("editor.inspector.physics.title"), &removed);
  if (removed) {
    remove_physics(context.session, index);
  } else if (open) {
    draw_physics_rows(context, index, body);
  }
}

void draw_group_collision_layer(EditorSession& session) {
  std::vector<std::string> names;
  for (const auto id : session.selected().ids()) {
    const auto& props = session.document().node(id).props;
    if (has_physics(props)) names.push_back(layer_preview(props));
  }
  if (names.empty()) return;
  const auto preview = shared_value(names).value_or(tr("editor.inspector.mixed"));
  const auto* picked = pick_layer(session.collision_layers(), preview);
  if (picked) session.set_collision_layer(session.selected().ids(), *picked);
}

void draw_wheel(EditorSession& session, int index) {
  const NodeDesc& node = session.document().node(index);
  if (node.parent == kNoIndex) return;
  const auto parent_body = get_string(session.document().node(node.parent).props, props::kBody);
  const bool is_wheel = get_bool(node.props, props::kWheel).value_or(false);
  if (!wheel_card_shown(parent_body, is_wheel)) return;
  if (!component_header(ICON_FA_CAR, tr("editor.inspector.wheel.title"))) return;
  bool wheel = is_wheel;
  property_label(tr("editor.inspector.wheel.is_wheel"));
  if (ImGui::Checkbox(kWheelId, &wheel)) set_or_remove(session, index, props::kWheel, flag_or_nothing(wheel, true));
}

void add_body_menu(EditorSession& session, int index) {
  for (int i = 0; i < static_cast<int>(std::size(props::kBodyTypes)); ++i) {
    if (ImGui::MenuItem(props::kBodyTypes[i])) {
      set_or_remove(session, index, props::kBody, PropertyValue(std::string(props::kBodyTypes[i])));
    }
    tooltip(tr(kBodyDescriptions[i]));
  }
}

}  // namespace redgem::ui::inspector
