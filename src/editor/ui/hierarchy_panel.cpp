#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/ui/hierarchy_rows.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

using namespace editor_config;
using namespace hierarchy;

constexpr float kRootDropMinHeight = 40.0f;  // pixels: the empty space below the tree is never smaller
constexpr float kRootDropMinWidth = 1.0f;    // ImGui refuses zero-sized buttons
constexpr const char* kCreatePopup = "create";
constexpr const char* kEmptySpaceMenu = "empty_menu";
constexpr const char* kSearchId = "##search";
constexpr const char* kTreeId = "tree";
constexpr const char* kEmptySpaceId = "##empty";

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
  ImGui::BeginDisabled(!context.session.editing());
  if (icon_button(ICON_FA_PLUS, tr("editor.hierarchy.create_tooltip"))) ImGui::OpenPopup(kCreatePopup);
  ImGui::EndDisabled();
  popup(kCreatePopup, [&context] { draw_create_menu(context); });
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kFillWidth);
  const char* hint = icon_label(ICON_FA_MAGNIFYING_GLASS, "editor.hierarchy.search");
  ImGui::InputTextWithHint(kSearchId, hint, &context.state.hierarchy.filter);
}

// Every object, under its parent.
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
  ImGui::BeginDisabled(!context.session.editing());
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
