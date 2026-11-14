// Inspector card of an object's joint: what holds it to another object or to the world (a door's
// hinge, a drawer's slide, a lamp's chain), saved as the object's `joint` properties.

#include <algorithm>
#include <cfloat>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/inspector_logic.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/style.hpp"
#include "scene/props.hpp"

namespace redgem::ui::inspector {
namespace {

using namespace editor_config;
using namespace inspector_logic;

// The keys of what each of props::kJointKinds is for, in their order.
constexpr const char* kKindDescriptions[] = {"editor.inspector.joint.fixed_kind", "editor.inspector.joint.hinge_kind",
                                             "editor.inspector.joint.slider_kind", "editor.inspector.joint.point_kind",
                                             "editor.inspector.joint.distance_kind"};
static_assert(std::size(kKindDescriptions) == std::size(props::kJointKinds), "a description for every joint kind");
constexpr const char* kKindId = "##joint_kind";
constexpr const char* kPartnerId = "##joint_to";
constexpr const char* kAnchorId = "##joint_anchor";
constexpr const char* kAxisId = "##joint_axis";
constexpr const char* kOtherAnchorId = "##joint_other_anchor";
constexpr const char* kLimitedId = "##joint_limited";
constexpr const char* kLimitsId = "##joint_limits";
constexpr const char* kMotorSpeedId = "##joint_motor_speed";
constexpr const char* kMotorTorqueId = "##joint_motor_torque";
constexpr const char* kMinId = "##joint_min";
constexpr const char* kMaxId = "##joint_max";
constexpr const char* kSpringId = "##joint_spring";
constexpr const char* kDampingId = "##joint_damping";
constexpr const char* kBreakForceId = "##joint_break_force";
constexpr int kLimitEnds = 2;  // low, high
constexpr int kVectorParts = 3;

// A number row of the card: `key` changes while the number is dragged, never below `lowest`.
struct NumberRow {
  const char* label;
  const char* id;
  const char* key;
  float value;
  float lowest;
  const char* format;
};

void number_row(PanelContext& context, int index, NumberRow row) {
  property_label(tr(row.label));
  if (!ImGui::DragFloat(row.id, &row.value, kDragSpeedNumber, row.lowest, FLT_MAX, row.format)) return;
  streamed_edit(context);
  context.session.document().set_prop(index, row.key, static_cast<double>(std::max(row.value, row.lowest)), false);
  context.session.refresh_node(index);
}

void vector_row(PanelContext& context, int index, const char* label, const char* id,
                const char* const (&keys)[kVectorParts], Vec3 value) {
  if (!drag_vec3(tr(label), id, value, kDragSpeedPosition)) return;
  streamed_edit(context);
  const float parts[] = {value.x, value.y, value.z};
  for (int axis = 0; axis < kVectorParts; ++axis) {
    context.session.document().set_prop(index, keys[axis], static_cast<double>(parts[axis]), false);
  }
  context.session.refresh_node(index);
}

// Each of the rows below that can change what the card shows returns true when it did: the card
// is then drawn again from the new properties on the next frame.
bool draw_kind_row(EditorSession& session, int index, const JointConfig& joint) {
  const int count = static_cast<int>(std::size(props::kJointKinds));
  int kind = joint.known_kind ? static_cast<int>(joint.kind) : kNoIndex;
  property_label(tr("editor.inspector.joint.kind"));
  if (ImGui::Combo(kKindId, &kind, props::kJointKinds, count)) {
    session.set_joint_kind(index, static_cast<JointKind>(kind));
    return true;
  }
  if (kind != kNoIndex) tooltip(tr(kKindDescriptions[kind]));
  return false;
}

std::string partner_name(const SceneDocument& document, const std::string& other) {
  if (other.empty()) return tr("editor.inspector.joint.world");
  for (const NodeDesc& node : document.desc().nodes) {
    if (node.stable_id == other) return node.name;
  }
  return tr("editor.inspector.joint.missing");
}

// Whether a node is offered as the other end of `index`'s joint: another node with an identity.
bool offered(const SceneDocument& document, int index, int node) {
  return node != index && !document.node(node).stable_id.empty();
}

// The other object chosen in the list: kNoIndex for the world; nullopt when nothing was chosen.
std::optional<int> pick_partner(const SceneDocument& document, int index, const std::string& other) {
  std::optional<int> picked;
  if (ImGui::Selectable(tr("editor.inspector.joint.world"), other.empty())) picked = kNoIndex;
  for (int node = 0; node < document.node_count(); ++node) {
    if (!offered(document, index, node)) continue;
    const NodeDesc& desc = document.node(node);
    if (ImGui::Selectable(desc.name.c_str(), desc.stable_id == other)) picked = node;
  }
  return picked;
}

bool draw_partner_row(EditorSession& session, int index, const JointConfig& joint) {
  property_label(tr("editor.inspector.joint.connected"));
  if (!ImGui::BeginCombo(kPartnerId, partner_name(session.document(), joint.other).c_str())) return false;
  const std::optional<int> picked = pick_partner(session.document(), index, joint.other);
  ImGui::EndCombo();
  if (picked) session.set_joint_partner(index, *picked);
  return picked.has_value();
}

void set_limits(EditorSession& session, int index, std::optional<std::pair<float, float>> limits) {
  SceneDocument& doc = session.document();
  doc.checkpoint();
  if (limits) {
    doc.set_prop(index, props::kJointLow, static_cast<double>(limits->first), false);
    doc.set_prop(index, props::kJointHigh, static_cast<double>(limits->second), false);
  } else {
    for (const char* key : {props::kJointLow, props::kJointHigh}) doc.remove_prop(index, key, false);
  }
  doc.drop_checkpoint_if_unchanged();
  session.refresh_preview();
}

void draw_limit_numbers(PanelContext& context, int index, std::pair<float, float> limits) {
  float ends[kLimitEnds] = {limits.first, limits.second};
  property_label(tr("editor.inspector.joint.low_high"));
  if (!ImGui::DragFloat2(kLimitsId, ends, kDragSpeedNumber)) return;
  streamed_edit(context);
  SceneDocument& doc = context.session.document();
  doc.set_prop(index, props::kJointLow, static_cast<double>(ends[0]), false);
  doc.set_prop(index, props::kJointHigh, static_cast<double>(ends[1]), false);
  context.session.refresh_node(index);
}

bool draw_limit_rows(PanelContext& context, int index, const JointConfig& joint) {
  bool limited = joint.limits.has_value();
  property_label(tr(joint.kind == JointKind::Hinge ? "editor.inspector.joint.limits_degrees"
                                                   : "editor.inspector.joint.limits_meters"));
  if (ImGui::Checkbox(kLimitedId, &limited)) {
    set_limits(context.session, index, limited ? std::optional(default_joint_limits(joint.kind)) : std::nullopt);
    return true;
  }
  if (joint.limits) draw_limit_numbers(context, index, *joint.limits);
  return false;
}

void draw_motor_rows(PanelContext& context, int index, const JointConfig& joint) {
  number_row(context, index,
             {"editor.inspector.joint.motor_speed", kMotorSpeedId, props::kJointMotorSpeed, joint.motor_speed, -FLT_MAX,
              "%.1f deg/s"});
  number_row(context, index,
             {"editor.inspector.joint.motor_torque", kMotorTorqueId, props::kJointMotorTorque, joint.motor_torque, 0.0f,
              "%.1f N m"});
  tooltip(tr("editor.inspector.joint.motor_tooltip"));
}

void draw_distance_rows(PanelContext& context, int index, const JointConfig& joint) {
  vector_row(context, index, "editor.inspector.joint.other_anchor", kOtherAnchorId, props::kJointOtherAnchor,
             joint.other_anchor);
  number_row(context, index,
             {"editor.inspector.joint.min", kMinId, props::kJointMin, joint.min_distance, 0.0f, "%.2f m"});
  number_row(
      context, index,
      {"editor.inspector.joint.max", kMaxId, props::kJointMax, joint.max_distance.value_or(0.0f), 0.0f, "%.2f m"});
  tooltip(tr("editor.inspector.joint.max_tooltip"));
  number_row(
      context, index,
      {"editor.inspector.joint.spring", kSpringId, props::kJointSpring, joint.spring_frequency, 0.0f, "%.2f Hz"});
  number_row(context, index,
             {"editor.inspector.joint.damping", kDampingId, props::kJointDamping, joint.spring_damping, 0.0f, "%.2f"});
}

// The rows of what the kind takes, until one of them changes what the card shows.
bool draw_kind_settings(PanelContext& context, int index, const JointConfig& joint) {
  vector_row(context, index, "editor.inspector.joint.anchor", kAnchorId, props::kJointAnchor, joint.anchor);
  if (joint_has_axis(joint.kind)) {
    vector_row(context, index, "editor.inspector.joint.axis", kAxisId, props::kJointAxis, joint.axis);
    if (draw_limit_rows(context, index, joint)) return true;
  }
  if (joint_has_motor(joint.kind)) draw_motor_rows(context, index, joint);
  if (joint_is_distance(joint.kind)) draw_distance_rows(context, index, joint);
  return false;
}

void draw_joint_warnings(const PropertyMap& props, const JointConfig& joint) {
  ImGui::PushStyleColor(ImGuiCol_Text, warning_color());
  if (!body_config_from_props(props).wants_physics()) {
    ImGui::TextWrapped(ICON_FA_TRIANGLE_EXCLAMATION "  %s", tr("editor.inspector.joint.no_body"));
  }
  for (const std::string& warning : joint.warnings) {
    ImGui::TextWrapped(ICON_FA_TRIANGLE_EXCLAMATION "  %s", warning.c_str());
  }
  ImGui::PopStyleColor();
}

void draw_joint_rows(PanelContext& context, int index, const JointConfig& joint) {
  if (draw_kind_row(context.session, index, joint)) return;
  if (draw_partner_row(context.session, index, joint)) return;
  if (joint.known_kind && draw_kind_settings(context, index, joint)) return;
  number_row(context, index,
             {"editor.inspector.joint.break_force", kBreakForceId, props::kJointBreakForce, joint.break_force, 0.0f,
              "%.0f N"});
  tooltip(tr("editor.inspector.joint.break_force_tooltip"));
  draw_joint_warnings(context.session.document().node(index).props, joint);
}

}  // namespace

void draw_joint(PanelContext& context, int index) {
  const PropertyMap& props = context.session.document().node(index).props;
  const std::optional<JointConfig> joint = joint_config_from_props(props);
  if (!joint) return;
  bool removed = false;
  const bool open = component_header(ICON_FA_LINK, tr("editor.inspector.joint.title"), &removed);
  if (removed) {
    context.session.remove_joint(index);
  } else if (open) {
    draw_joint_rows(context, index, *joint);
  }
}

void add_joint_menu(EditorSession& session, int index) {
  for (int i = 0; i < static_cast<int>(std::size(props::kJointKinds)); ++i) {
    if (ImGui::MenuItem(props::kJointKinds[i])) session.add_joint(index, static_cast<JointKind>(i));
    tooltip(tr(kKindDescriptions[i]));
  }
}

}  // namespace redgem::ui::inspector
