#include "editor/ui/hierarchy_rows.hpp"

#include <imgui.h>

#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"
#include "scene/props.hpp"

namespace redgem::ui::hierarchy {
namespace {

using namespace editor_config;

constexpr const char* kNodeMenu = "node_menu";
constexpr const char* kNodeId = "##node";

const char* icon_of(const NodeDesc& node) {
  if (node.camera) return ICON_FA_VIDEO;
  if (node.light) return node.light->type == LightType::Directional ? ICON_FA_SUN : ICON_FA_LIGHTBULB;
  if (get_bool(node.props, props::kPrefab).value_or(false)) return ICON_FA_CUBES;
  if (node.mesh != kNoIndex) return ICON_FA_CUBE;
  return node.children.empty() ? ICON_FA_CIRCLE_DOT : ICON_FA_LAYER_GROUP;
}

// What can be done to the object itself.
void draw_node_actions(bool has_parent, int index, Pending& pending) {
  if (ImGui::MenuItem(duplicate_label(), tr(kKeysDuplicate))) pending = {Pending::Kind::Duplicate, index, kNoIndex, {}};
  if (ImGui::MenuItem(delete_label(), tr(kKeysDelete))) pending = {Pending::Kind::Delete, index, kNoIndex, {}};
  if (!has_parent) return;
  if (ImGui::MenuItem(icon_label(ICON_FA_ARROW_TURN_UP, "editor.hierarchy.move_to_top"))) {
    pending = {Pending::Kind::Reparent, index, kNoIndex, {}};
  }
}

void node_context_menu(PanelContext& context, int index, Pending& pending) {
  if (!ImGui::BeginPopupContextItem(kNodeMenu)) return;
  ImGui::BeginDisabled(!context.session.editing());
  draw_node_actions(context.session.document().node(index).parent != kNoIndex, index, pending);
  ImGui::Separator();
  menu(icon_label(ICON_FA_PLUS, "editor.hierarchy.create"), [&context] { draw_create_menu(context); });
  ImGui::EndDisabled();
  ImGui::EndPopup();
}

// The type icon, the name and a gem when the object has scripts.
void draw_label(const RowLabel& label) {
  ImGui::SameLine();
  ImGui::TextColored(label.template_object ? accent_color() : muted_color(), "%s", label.icon);
  ImGui::SameLine();
  if (label.dimmed()) {
    ImGui::TextColored(muted_color(), "%s", label.name.c_str());
  } else {
    ImGui::TextUnformatted(label.name.c_str());
  }
  if (!label.has_script) return;
  ImGui::SameLine();
  ImGui::TextColored(accent_color(), ICON_FA_GEM);
}

bool row_clicked() {
  return ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right);
}

bool row_double_clicked() { return ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left); }

// A click on the row just drawn selects its object; a double click asks to frame it.
void use_row_clicks(EditorSession& session, int index, HierarchyResult& result) {
  if (row_clicked()) session.select(index);
  if (row_double_clicked()) result.frame_requested = true;
}

// The row just drawn can be dragged onto another object (or onto the empty space) while editing.
void offer_row_drag(const EditorSession& session, int index, const RowLabel& label) {
  if (!session.editing()) return;
  if (!ImGui::BeginDragDropSource()) return;
  ImGui::SetDragDropPayload(kPayloadNode, &index, sizeof(index));
  ImGui::Text("%s  %s", label.icon, label.name.c_str());
  ImGui::EndDragDropSource();
}

}  // namespace

RowLabel label_of(const NodeDesc& node) {
  RowLabel label;
  label.name = node.name;
  label.icon = icon_of(node);
  label.template_object = get_bool(node.props, props::kPrefab).value_or(false);
  label.hidden = !get_bool(node.props, props::kVisible).value_or(true);
  label.has_script = get_string(node.props, props::kScript).has_value();
  return label;
}

void accept_drops(int target, Pending& pending) {
  if (!ImGui::BeginDragDropTarget()) return;
  if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayloadNode)) {
    pending = {Pending::Kind::Reparent, *static_cast<const int*>(payload->Data), target, {}};
  }
  const char* script = target != kNoIndex ? dropped_text(kPayloadScript) : nullptr;
  if (script != nullptr) pending = {Pending::Kind::AttachScript, target, kNoIndex, script};
  if (const char* model = dropped_text(kPayloadModel)) pending = {Pending::Kind::ImportModel, kNoIndex, target, model};
  ImGui::EndDragDropTarget();
}

bool draw_row(PanelContext& context, int index, int flags, HierarchyResult& result, Pending& pending) {
  EditorSession& session = context.session;
  const RowLabel label = label_of(session.document().node(index));
  if (session.selection() == index) flags |= ImGuiTreeNodeFlags_Selected;
  flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth;
  const bool open = ImGui::TreeNodeEx(kNodeId, flags);
  use_row_clicks(session, index, result);
  offer_row_drag(session, index, label);
  accept_drops(index, pending);
  node_context_menu(context, index, pending);
  draw_label(label);
  return open;
}

}  // namespace redgem::ui::hierarchy
