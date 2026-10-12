#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

#include "scene/props.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

using namespace editor_config;

constexpr float kRootDropMinHeight = 40.0f;  // pixels: the empty space below the tree is never smaller
constexpr float kRootDropMinWidth = 1.0f;    // ImGui refuses zero-sized buttons
constexpr const char* kCreatePopup = "create";
constexpr const char* kNodeMenu = "node_menu";
constexpr const char* kEmptySpaceMenu = "empty_menu";
constexpr const char* kSearchId = "##search";
constexpr const char* kTreeId = "tree";
constexpr const char* kNodeId = "##node";
constexpr const char* kEmptySpaceId = "##empty";

// Edits are collected while the tree is drawn and applied after it (they change indices).
struct Pending {
  enum class Kind { None, Duplicate, Delete, Reparent, AttachScript, ImportModel };
  Kind kind = Kind::None;
  int target = kNoIndex;
  int parent = kNoIndex;
  std::string file;
};

const char* icon_of(const NodeDesc& node) {
  if (node.camera) return ICON_FA_VIDEO;
  if (node.light) return node.light->type == LightType::Directional ? ICON_FA_SUN : ICON_FA_LIGHTBULB;
  if (get_bool(node.props, props::kPrefab).value_or(false)) return ICON_FA_CUBES;
  if (node.mesh != kNoIndex) return ICON_FA_CUBE;
  return node.children.empty() ? ICON_FA_CIRCLE_DOT : ICON_FA_LAYER_GROUP;
}

// Accepts nodes, scripts and models dropped on the last item (`target` kNoIndex = top level).
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

// What can be done to the object itself.
void draw_node_actions(bool has_parent, int index, Pending& pending) {
  if (ImGui::MenuItem(kDuplicateLabel, kKeysDuplicate)) pending = {Pending::Kind::Duplicate, index, kNoIndex, {}};
  if (ImGui::MenuItem(kDeleteLabel, kKeysDelete)) pending = {Pending::Kind::Delete, index, kNoIndex, {}};
  if (has_parent && ImGui::MenuItem(ICON_FA_ARROW_TURN_UP "  Move to top level")) {
    pending = {Pending::Kind::Reparent, index, kNoIndex, {}};
  }
}

void node_context_menu(PanelContext& context, int index, Pending& pending) {
  if (!ImGui::BeginPopupContextItem(kNodeMenu)) return;
  ImGui::BeginDisabled(context.session.play_state() != PlayState::Editing);
  draw_node_actions(context.session.document().node(index).parent != kNoIndex, index, pending);
  ImGui::Separator();
  menu(ICON_FA_PLUS "  Create", [&context] { draw_create_menu(context); });
  ImGui::EndDisabled();
  ImGui::EndPopup();
}

// What a row shows of its object (copied: the row's menu can create objects, which reallocates
// the node list).
struct RowLabel {
  std::string name;
  const char* icon = "";
  bool template_object = false;
  bool hidden = false;
  bool has_script = false;
};

RowLabel label_of(const NodeDesc& node) {
  RowLabel label;
  label.name = node.name;
  label.icon = icon_of(node);
  label.template_object = get_bool(node.props, props::kPrefab).value_or(false);
  label.hidden = !get_bool(node.props, props::kVisible).value_or(true);
  label.has_script = get_string(node.props, props::kScript).has_value();
  return label;
}

// The type icon, the name and a gem when the object has scripts.
void draw_label(const RowLabel& label) {
  ImGui::SameLine();
  ImGui::TextColored(label.template_object ? accent_color() : muted_color(), "%s", label.icon);
  ImGui::SameLine();
  if (label.hidden || label.template_object) {
    ImGui::TextColored(muted_color(), "%s", label.name.c_str());
  } else {
    ImGui::TextUnformatted(label.name.c_str());
  }
  if (!label.has_script) return;
  ImGui::SameLine();
  ImGui::TextColored(accent_color(), ICON_FA_GEM);
}

// A click on the row just drawn selects its object; a double click asks to frame it.
void use_row_clicks(EditorSession& session, int index, HierarchyResult& result) {
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
    session.select(index);
  }
  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) result.frame_requested = true;
}

// The row just drawn can be dragged onto another object (or onto the empty space) while editing.
void offer_row_drag(const EditorSession& session, int index, const RowLabel& label) {
  if (session.play_state() != PlayState::Editing || !ImGui::BeginDragDropSource()) return;
  ImGui::SetDragDropPayload(kPayloadNode, &index, sizeof(index));
  ImGui::Text("%s  %s", label.icon, label.name.c_str());
  ImGui::EndDragDropSource();
}

// One row: a tree node that selects, frames, drags, takes drops and opens the object's menu.
bool draw_row(PanelContext& context, int index, ImGuiTreeNodeFlags flags, HierarchyResult& result, Pending& pending) {
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

void draw_tree(PanelContext& context, int index, HierarchyResult& result, Pending& pending) {
  const NodeDesc& node = context.session.document().node(index);
  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
  if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
  // Keyed by name (unique in the editor) so open/closed state follows objects when indices shift.
  ImGui::PushID(node.name.c_str());
  if (draw_row(context, index, flags, result, pending)) {
    // Copy: actions are deferred, but keep the loop independent of the document anyway.
    const std::vector<int> children = context.session.document().node(index).children;
    for (const int child : children) draw_tree(context, child, result, pending);
    ImGui::TreePop();
  }
  ImGui::PopID();
}

// What each kind of pending edit does, in Pending::Kind's order.
void apply_nothing(PanelContext& /*context*/, const Pending& /*pending*/) {}

void apply_duplicate(PanelContext& context, const Pending& pending) {
  context.session.select(pending.target);
  context.session.duplicate_selection();
}

void apply_delete(PanelContext& context, const Pending& pending) {
  context.session.select(pending.target);
  context.session.delete_selection();
}

void apply_reparent(PanelContext& context, const Pending& pending) {
  context.session.reparent(pending.target, pending.parent);
}

void apply_attach_script(PanelContext& context, const Pending& pending) {
  context.session.attach_script(pending.target, pending.file);
  context.session.select(pending.target);
}

void apply_import_model(PanelContext& context, const Pending& pending) {
  import_model(context.session, pending.file, context.spawn_point, pending.parent);
}

using ApplyPending = void (*)(PanelContext&, const Pending&);
constexpr ApplyPending kApplyPending[] = {apply_nothing,  apply_duplicate,     apply_delete,
                                          apply_reparent, apply_attach_script, apply_import_model};

void apply(PanelContext& context, const Pending& pending) {
  kApplyPending[static_cast<std::size_t>(pending.kind)](context, pending);
}

// The "+" button and the search field.
void draw_search_bar(PanelContext& context) {
  ImGui::BeginDisabled(context.session.play_state() != PlayState::Editing);
  if (icon_button(ICON_FA_PLUS, "Create an object")) ImGui::OpenPopup(kCreatePopup);
  ImGui::EndDisabled();
  popup(kCreatePopup, [&context] { draw_create_menu(context); });
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::InputTextWithHint(kSearchId, ICON_FA_MAGNIFYING_GLASS "  Search", &context.state.hierarchy.filter);
}

// The whole tree, or a flat list of the objects whose name matches the search.
void draw_whole_tree(PanelContext& context, HierarchyResult& result, Pending& pending) {
  const SceneDocument& doc = context.session.document();
  for (int i = 0; i < doc.node_count(); ++i) {
    if (doc.node(i).parent == kNoIndex) draw_tree(context, i, result, pending);
  }
}

// A flat list of the objects whose name contains `wanted` (lowercase).
void draw_matches(PanelContext& context, const std::string& wanted, HierarchyResult& result, Pending& pending) {
  const SceneDocument& doc = context.session.document();
  for (int i = 0; i < doc.node_count(); ++i) {
    if (!editor_logic::name_contains(doc.node(i).name, wanted)) continue;
    ImGui::PushID(i);
    draw_row(context, i, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, result, pending);
    ImGui::PopID();
  }
}

// The whole tree, or a flat list of the objects whose name matches the search.
void draw_objects(PanelContext& context, HierarchyResult& result, Pending& pending) {
  const std::string& filter = context.state.hierarchy.filter;
  if (filter.empty()) {
    draw_whole_tree(context, result, pending);
  } else {
    draw_matches(context, editor_logic::lowercase(filter), result, pending);
  }
}

// The empty space below the tree: drop here to move objects to the top level, click to deselect.
void draw_empty_space(PanelContext& context, Pending& pending) {
  const ImVec2 free = ImGui::GetContentRegionAvail();
  const ImVec2 size(std::max(free.x, kRootDropMinWidth), std::max(free.y, kRootDropMinHeight));
  if (ImGui::InvisibleButton(kEmptySpaceId, size)) context.session.select(kNoObject);
  accept_drops(kNoIndex, pending);
  if (!ImGui::BeginPopupContextItem(kEmptySpaceMenu)) return;
  ImGui::BeginDisabled(context.session.play_state() != PlayState::Editing);
  draw_create_menu(context);
  ImGui::EndDisabled();
  ImGui::EndPopup();
}

}  // namespace

HierarchyResult draw_hierarchy(PanelContext& context) {
  HierarchyResult result;
  Pending pending;
  ImGui::Begin(kHierarchyWindow);
  draw_search_bar(context);
  ImGui::Separator();
  ImGui::BeginChild(kTreeId);
  draw_objects(context, result, pending);
  draw_empty_space(context, pending);
  ImGui::EndChild();
  ImGui::End();
  apply(context, pending);
  return result;
}

}  // namespace redgem::ui
