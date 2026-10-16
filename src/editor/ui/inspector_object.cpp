// Inspector cards of every object: its name, tag and flags, and its placement.

#include <cfloat>

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

constexpr const char* kNameId = "##name";
constexpr const char* kTagId = "##tag";
constexpr const char* kPositionId = "##position";
constexpr const char* kRotationId = "##rotation";
constexpr const char* kScaleId = "##scale";

const char* object_icon(const NodeDesc& node) {
  if (node.camera) return ICON_FA_VIDEO;
  if (node.light) return node.light->type == LightType::Directional ? ICON_FA_SUN : ICON_FA_LIGHTBULB;
  if (node.mesh != kNoIndex) return ICON_FA_CUBE;
  return ICON_FA_CIRCLE_DOT;
}

// The object's icon and its name. A name already taken gets a number; an empty one is refused.
void draw_name_field(PanelContext& context, int index) {
  const NodeDesc& node = context.session.document().node(index);
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(accent_color(), "%s", object_icon(node));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kFillWidth);
  const auto name = text_field(context, kNameId, index, node.name);
  if (!name) return;
  if (!name->text.empty())
    context.session.document().set_name(name->object, context.session.document().unique_name(name->text));
  context.session.refresh_preview();
}

void draw_tag_field(PanelContext& context, int index) {
  const std::string current = get_string(context.session.document().node(index).props, props::kTag).value_or("");
  property_label(tr("editor.inspector.object.tag"));
  const auto tag = text_field(context, kTagId, index, current, tr("editor.inspector.object.tag_hint"));
  if (tag) set_or_remove(context.session, tag->object, props::kTag, text_or_nothing(tag->text));
}

// Visible and Template, side by side.
void draw_flags(EditorSession& session, int index) {
  const PropertyMap& props = session.document().node(index).props;
  bool visible = get_bool(props, props::kVisible).value_or(true);
  if (ImGui::Checkbox(tr("editor.inspector.object.visible"), &visible)) {
    set_or_remove(session, index, props::kVisible, flag_or_nothing(!visible, false));
  }
  ImGui::SameLine();
  bool prefab = get_bool(props, props::kPrefab).value_or(false);
  if (ImGui::Checkbox(tr("editor.inspector.object.template"), &prefab)) {
    set_or_remove(session, index, props::kPrefab, flag_or_nothing(prefab, true));
  }
  tooltip(tr("editor.inspector.object.template_tooltip"));
}

bool drag_rotation(Quat& rotation) {
  Vec3 euler = quat_to_euler_degrees(rotation);
  if (!drag_vec3(tr("editor.inspector.transform.rotation"), kRotationId, euler, kDragSpeedRotation)) return false;
  rotation = quat_from_euler_degrees(euler);
  return true;
}

// Only touches the scale when it was itself edited: a mirrored object (negative scale) must survive a move.
bool drag_scale(Vec3& scale) {
  Vec3 edited = scale;
  const char* label = tr("editor.inspector.transform.scale");
  if (!drag_vec3(label, kScaleId, edited, kDragSpeedScale, kMinScale, FLT_MAX)) return false;
  scale = editor_logic::at_least(edited, kMinScale);
  return true;
}

}  // namespace

void draw_header(PanelContext& context, int index) {
  draw_name_field(context, index);
  draw_tag_field(context, index);
  draw_flags(context.session, index);
}

void draw_transform(PanelContext& context, int index) {
  if (!component_header(ICON_FA_UP_DOWN_LEFT_RIGHT, tr("editor.inspector.transform.title"))) return;
  SceneDocument& doc = context.session.document();
  Transform local = doc.node(index).local;
  bool changed = drag_vec3(tr("editor.inspector.transform.position"), kPositionId, local.position, kDragSpeedPosition);
  changed |= drag_rotation(local.rotation);
  changed |= drag_scale(local.scale);
  if (!changed) return;
  streamed_edit(context);
  doc.set_local(index, local, false);
  context.session.refresh_node(index);
}

}  // namespace redgem::ui::inspector
