// Part of EditorApp: the dock layout, and the panels and tabs in it.

#include "editor/editor_app.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr float kSplitRatio = 0.5f;  // a split tab gets half of its group
constexpr int kMinTabsToSplit = 2;   // a tab needs a neighbour to split away from
constexpr const char* kDockspaceId = "EditorDock";

// An empty dock space as big as the window, ready to be split.
void start_layout(ImGuiID dock) {
  ImGui::DockBuilderRemoveNode(dock);
  ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
}

// Moves a tab into a new group on the right half of its own. Returns that group's dock.
ImGuiID split_to_the_right(const ImGuiDockNode& group, const std::string& window_name) {
  ImGuiID right = 0;
  ImGuiID left = 0;
  ImGui::DockBuilderSplitNode(group.ID, ImGuiDir_Right, kSplitRatio, &right, &left);
  ImGui::DockBuilderDockWindow(window_name.c_str(), right);
  ImGui::DockBuilderFinish(ImGui::GetID(kDockspaceId));
  return right;
}

// A window that exists and sits in a dock group.
bool in_dock_group(const ImGuiWindow* window) { return window != nullptr && window->DockNode != nullptr; }

// A window that exists and is docked somewhere.
bool docked(const ImGuiWindow* window) { return window != nullptr && window->DockId != 0; }

}  // namespace

void EditorApp::draw_dockspace() {
  const ImGuiViewport* main = ImGui::GetMainViewport();
  const ImGuiID dock = ImGui::GetID(kDockspaceId);
  if (!layout_built_) {
    layout_built_ = true;
    build_default_layout(dock);
  }
  ImGui::DockSpaceOverViewport(dock, main);
  apply_pending_split();
}

// Hierarchy on the left, Inspector on the right, Project and Console below the Scene.
void EditorApp::build_default_layout(unsigned int dock) {
  start_layout(dock);
  ImGuiID center = dock;
  const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, kHierarchyWidth, nullptr, &center);
  const ImGuiID right =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, editor_logic::inspector_split_ratio(), nullptr, &center);
  const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, kBottomHeight, nullptr, &center);
  ImGui::DockBuilderDockWindow(kHierarchyWindow, left);
  ImGui::DockBuilderDockWindow(kInspectorWindow, right);
  ImGui::DockBuilderDockWindow(kProjectWindow, bottom);
  ImGui::DockBuilderDockWindow(kConsoleWindow, bottom);
  ImGui::DockBuilderDockWindow(kViewportWindow, center);
  ImGui::DockBuilderFinish(dock);
  center_dock_ = center;
}

void EditorApp::split_script_right(const std::string& file_name) { pending_split_ = ui::script_window_name(file_name); }

void EditorApp::apply_pending_split() {
  if (!pending_split_) return;
  const std::string name = *pending_split_;
  pending_split_.reset();
  ImGuiWindow* window = ImGui::FindWindowByName(name.c_str());
  if (!in_dock_group(window)) return;
  if (window->DockNode->Windows.Size < kMinTabsToSplit) {
    log::info("drag another tab here first: a single tab has nothing to split from");
    return;
  }
  script_dock_ = split_to_the_right(*window->DockNode, name);
}

unsigned int EditorApp::new_script_dock() const {
  // New scripts open next to the last script used (like a code editor's active group), else next
  // to the Scene.
  if (script_dock_exists()) return script_dock_;
  if (const ImGuiWindow* scene = ImGui::FindWindowByName(kViewportWindow); docked(scene)) return scene->DockId;
  return center_dock_;
}

bool EditorApp::script_dock_exists() const {
  return script_dock_ != 0 && ImGui::DockBuilderGetNode(script_dock_) != nullptr;
}

void EditorApp::draw_panels() {
  ui::PanelContext context = panel_context();
  draw_side_panels(context);
  draw_viewport_window();  // before the script tabs: the Scene tab comes first
  draw_script_tabs();
  if (context.files_changed) rescan_files();
  take_scene_requests(context);
}

void EditorApp::draw_side_panels(ui::PanelContext& context) {
  if (panels_.hierarchy) draw_hierarchy_panel(context);
  if (panels_.inspector) ui::draw_inspector(context);
  // Whatever is selected, and whether or not the Inspector is shown: a drag is over once its
  // widget is let go.
  panel_state_.inspector.edits.end_frame(ImGui::IsAnyItemActive());
  if (!panels_.bottom) return;
  ui::draw_project(context);
  ui::draw_console(*console_, panel_state_.console);
}

void EditorApp::draw_hierarchy_panel(ui::PanelContext& context) {
  const ui::HierarchyResult result = ui::draw_hierarchy(context);
  if (result.frame_requested) frame_selection();
  run_clipboard_command(result.clipboard, result.paste_parent);
}

void EditorApp::run_clipboard_command(ui::ClipboardCommand command, ObjectId paste_parent) {
  switch (command) {
    case ui::ClipboardCommand::Cut:
      return cut_selection();
    case ui::ClipboardCommand::Copy:
      return copy_selection();
    case ui::ClipboardCommand::Paste:
      return paste();
    case ui::ClipboardCommand::PasteInto:
      return paste_into(paste_parent);
    case ui::ClipboardCommand::None:
      return;
  }
}

void EditorApp::draw_script_tabs() {
  const ui::ScriptTabsResult tabs = ui::draw_script_tabs(*scripts_, panel_state_.script_tabs, new_script_dock());
  if (tabs.focused_dock != 0) script_dock_ = tabs.focused_dock;
  if (tabs.split_right) pending_split_ = tabs.split_right;
}

}  // namespace redgem
