// Inspector of several selected objects: what they all have (their placement, tag and flags), each
// field showing a value when they share it and a dash when they differ. An edit applies to every
// one of them, as one undo step.

#include <imgui.h>
#include <imgui_internal.h>

#include "core/filled_texts.hpp"
#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/inspector_logic.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"
#include "scene/props.hpp"

namespace redgem::ui::inspector {
namespace {

using namespace editor_config;
using namespace inspector_logic;

constexpr const char* kTagId = "##group_tag";
constexpr const char* kAxisId = "##axis";
constexpr const char* kNumberFormat = "%.3f";  // as ImGui shows a dragged number

// One number of a part changed: on which axis, to what.
struct AxisEdit {
  int axis = 0;
  float value = 0.0f;
};

// One part of a placement (see TransformPart) with how the Inspector drags it.
struct PartField {
  TransformPart part;
  const char* label;  // the key of its text
  const char* id;
  float speed;
};
constexpr PartField kPartFields[] = {
    {TransformPart::Position, "editor.inspector.transform.position", "##group_position", kDragSpeedPosition},
    {TransformPart::Rotation, "editor.inspector.transform.rotation", "##group_rotation", kDragSpeedRotation},
    {TransformPart::Scale, "editor.inspector.transform.scale", "##group_scale", kDragSpeedScale},
};

const std::vector<ObjectId>& selected(PanelContext& context) { return context.session.selected().ids(); }

std::vector<float> axis_values(const std::vector<Vec3>& values, int axis) {
  std::vector<float> numbers;
  numbers.reserve(values.size());
  for (const Vec3& value : values) numbers.push_back(axis_of(value, axis));
  return numbers;
}

// One axis of a part: the value the objects share, or a dash. Returns a value dragged to.
std::optional<float> drag_axis(const std::vector<Vec3>& values, int axis, float speed) {
  const std::optional<float> shared = shared_value(axis_values(values, axis));
  float value = shared.value_or(axis_of(values.front(), axis));
  ImGui::PushID(axis);
  const bool changed =
      ImGui::DragFloat(kAxisId, &value, speed, 0.0f, 0.0f, shared ? kNumberFormat : tr("editor.inspector.mixed"));
  ImGui::PopID();
  ImGui::PopItemWidth();
  return changed ? std::optional<float>(value) : std::nullopt;
}

// A labelled row of three numbers, one per axis, for values that may differ.
std::optional<AxisEdit> drag_mixed_vec3(const PartField& field, const std::vector<Vec3>& values) {
  property_label(tr(field.label));
  ImGui::PushID(field.id);
  ImGui::PushMultiItemsWidths(kAxes, ImGui::CalcItemWidth());
  std::optional<AxisEdit> edit;
  for (int axis = 0; axis < kAxes; ++axis) {
    if (axis > 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    if (const std::optional<float> value = drag_axis(values, axis, field.speed)) edit = AxisEdit{axis, *value};
  }
  ImGui::PopID();
  return edit;
}

void apply_axis(PanelContext& context, TransformPart part, const AxisEdit& edit) {
  streamed_edit(context);  // one undo step for every object, for as long as the field is held
  context.session.set_roots_axis(part, edit.axis, edit.value, kMinScale);
}

// The placements shown are the ones edited: of the selection's top level (a child moves with its
// selected parent, as with the gizmo).
void draw_part(PanelContext& context, const PartField& field) {
  std::vector<Vec3> values;
  for (const int id : context.session.selected_roots()) {
    values.push_back(part_of(context.session.document().node(id).local, field.part));
  }
  if (const std::optional<AxisEdit> edit = drag_mixed_vec3(field, values)) apply_axis(context, field.part, *edit);
}

// Sets a property of every one of `objects` (those still in the scene), or removes it for nullopt:
// one undo step.
void set_or_remove_all(PanelContext& context, const std::vector<ObjectId>& objects, const char* key,
                       const std::optional<PropertyValue>& value) {
  SceneDocument& doc = context.session.document();
  doc.checkpoint();
  for (const ObjectId id : objects) {
    if (!doc.has_node(id)) continue;
    if (value) {
      doc.set_prop(id, key, *value, false);
    } else {
      doc.remove_prop(id, key, false);
    }
  }
  context.session.refresh_preview();
}

// A flag property of every selected object (`absent` when it is not stored), shown half checked
// when they differ. Returns the value it was clicked to.
std::optional<bool> mixed_checkbox(PanelContext& context, const char* label_key, const char* key, bool absent) {
  std::vector<bool> values;
  for (const ObjectId id : selected(context)) {
    values.push_back(get_bool(context.session.document().node(id).props, key).value_or(absent));
  }
  const std::optional<bool> shared = shared_value(values);
  bool value = shared.value_or(false);
  ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, !shared);
  const bool clicked = ImGui::Checkbox(tr(label_key), &value);
  ImGui::PopItemFlag();
  return clicked ? std::optional<bool>(value) : std::nullopt;
}

void draw_group_flags(PanelContext& context) {
  if (const std::optional<bool> visible =
          mixed_checkbox(context, "editor.inspector.object.visible", props::kVisible, true)) {
    set_or_remove_all(context, selected(context), props::kVisible, flag_or_nothing(!*visible, false));
  }
  ImGui::SameLine();
  if (const std::optional<bool> prefab =
          mixed_checkbox(context, "editor.inspector.object.template", props::kPrefab, false)) {
    set_or_remove_all(context, selected(context), props::kPrefab, flag_or_nothing(*prefab, true));
  }
}

void draw_group_tag(PanelContext& context) {
  std::vector<std::string> tags;
  for (const ObjectId id : selected(context)) {
    tags.push_back(get_string(context.session.document().node(id).props, props::kTag).value_or(""));
  }
  const std::optional<std::string> shared = shared_value(tags);
  property_label(tr("editor.inspector.object.tag"));
  const char* hint = tr(shared ? "editor.inspector.object.tag_hint" : "editor.inspector.mixed");
  // Typing goes to the objects selected when it began, whatever is selected when it is committed.
  const bool typing = context.state.inspector.text_fields[ImGui::GetID(kTagId)].editing;
  const std::vector<ObjectId> targets = context.state.inspector.group_tag.follow(typing, selected(context));
  const auto tag = text_field(context, kTagId, context.session.selection(), shared.value_or(""), hint);
  if (tag) set_or_remove_all(context, targets, props::kTag, text_or_nothing(tag->text));
}

}  // namespace

void draw_group(PanelContext& context) {
  static FilledTexts title("editor.inspector.group.title", "count");
  ImGui::TextColored(accent_color(), "%s", ICON_FA_OBJECT_GROUP);
  ImGui::SameLine();
  ImGui::TextUnformatted(title.with(static_cast<long long>(selected(context).size())));
  draw_group_tag(context);
  draw_group_flags(context);
  if (!component_header(ICON_FA_UP_DOWN_LEFT_RIGHT, tr("editor.inspector.transform.title"))) return;
  for (const PartField& field : kPartFields) draw_part(context, field);
}

}  // namespace redgem::ui::inspector
