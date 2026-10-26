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

// What the clipboard can do with the object (and the rest of the selection).
void draw_clipboard_actions(int index, HierarchyResult& result) {
  if (ImGui::MenuItem(cut_label(), tr(kKeysCut))) result.clipboard = ClipboardCommand::Cut;
  if (ImGui::MenuItem(copy_label(), tr(kKeysCopy))) result.clipboard = ClipboardCommand::Copy;
  if (ImGui::MenuItem(paste_label(), tr(kKeysPaste))) result.clipboard = ClipboardCommand::Paste;
  if (ImGui::MenuItem(paste_into_label(), tr(kKeysPasteInto))) {
    result.clipboard = ClipboardCommand::PasteInto;
    result.paste_parent = index;
  }
}

// What can be done to the object itself (and the rest of the selection).
void draw_node_actions(bool has_parent, int index, Pending& pending) {
  if (ImGui::MenuItem(duplicate_label(), tr(kKeysDuplicate))) pending = {Pending::Kind::Duplicate, index, kNoIndex, {}};
  if (ImGui::MenuItem(delete_label(), tr(kKeysDelete))) pending = {Pending::Kind::Delete, index, kNoIndex, {}};
  if (!has_parent) return;
  if (ImGui::MenuItem(icon_label(ICON_FA_ARROW_TURN_UP, "editor.hierarchy.move_to_top"))) {
    pending = {Pending::Kind::Reparent, index, kNoIndex, {}};
  }
}

void node_context_menu(PanelContext& context, int index, HierarchyResult& result, Pending& pending) {
  if (!ImGui::BeginPopupContextItem(kNodeMenu)) return;
  ImGui::BeginDisabled(!context.session.editing());
  draw_clipboard_actions(index, result);
  ImGui::Separator();
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

bool row_double_clicked() { return ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left); }

// A left click selects the row's object as the keys held say (see click_mode). On a row of a
// selection of several, a plain one waits for the button to be let go (see GroupPress).
void click_row(PanelContext& context, int index) {
  const ImGuiIO& io = ImGui::GetIO();
  const ClickMode mode = click_mode(io.KeyCtrl, io.KeyShift);
  HierarchyState& state = context.state.hierarchy;
  if (GroupPress::waits(context.session.selected(), index, mode)) {
    state.press = GroupPress{index, false};
  } else {
    context.session.click_select(index, mode, state.rows);
  }
}

// A right click keeps a selection the row is part of (its menu acts on all of it); otherwise it
// selects the row's object alone.
void right_click_row(EditorSession& session, int index) {
  if (!session.selected().contains(index)) session.select(index);
}

// A click on the row just drawn selects its object; a double click asks to frame it.
void use_row_clicks(PanelContext& context, int index, HierarchyResult& result) {
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) click_row(context, index);
  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) right_click_row(context.session, index);
  if (row_double_clicked()) result.frame_requested = true;
}

// The row just drawn can be dragged onto another object (or onto the empty space) while editing;
// when it is part of the selection, the whole selection goes.
void offer_row_drag(PanelContext& context, int index, const RowLabel& label) {
  if (!context.session.editing()) return;
  if (!ImGui::BeginDragDropSource()) return;
  context.state.hierarchy.press.dragged = true;
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
  if (session.selected().contains(index)) flags |= ImGuiTreeNodeFlags_Selected;
  flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth;
  const bool open = ImGui::TreeNodeEx(kNodeId, flags);
  context.state.hierarchy.rows_drawn.push_back(index);
  use_row_clicks(context, index, result);
  offer_row_drag(context, index, label);
  accept_drops(index, pending);
  node_context_menu(context, index, result, pending);
  draw_label(label);
  return open;
}

}  // namespace redgem::ui::hierarchy
