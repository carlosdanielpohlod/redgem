#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>

#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

// State kept between frames. Reset when another game is opened (see reset_hierarchy_state).
std::string g_filter;


using namespace editor_config;

constexpr float kRootDropMinHeight = 40.0f;
constexpr float kRootDropMinWidth = 1.0f;  // ImGui refuses zero-sized buttons

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
  if (get_bool(node.props, config::kPropPrefab).value_or(false)) return ICON_FA_CUBES;
  if (node.mesh != kNoIndex) return ICON_FA_CUBE;
  return node.children.empty() ? ICON_FA_CIRCLE_DOT : ICON_FA_LAYER_GROUP;
}

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
  return text;
}

// Accepts nodes, scripts and models dropped on the last item (`target` kNoIndex = top level).
void accept_drops(int target, Pending& pending) {
  if (!ImGui::BeginDragDropTarget()) return;
  if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayloadNode)) {
    pending = {Pending::Kind::Reparent, *static_cast<const int*>(payload->Data), target, {}};
  }
  if (target != kNoIndex) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayloadScript)) {
      pending = {Pending::Kind::AttachScript, target, kNoIndex, static_cast<const char*>(payload->Data)};
    }
  }
  if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayloadModel)) {
    pending = {Pending::Kind::ImportModel, kNoIndex, target, static_cast<const char*>(payload->Data)};
  }
  ImGui::EndDragDropTarget();
}

void node_context_menu(PanelContext& context, int index, Pending& pending) {
  if (!ImGui::BeginPopupContextItem("node_menu")) return;
  const bool editing = context.session.play_state() == PlayState::Editing;
  ImGui::BeginDisabled(!editing);
  if (ImGui::MenuItem(ICON_FA_CLONE "  Duplicate", "Ctrl+D")) pending = {Pending::Kind::Duplicate, index, kNoIndex, {}};
  if (ImGui::MenuItem(ICON_FA_TRASH "  Delete", "Del")) pending = {Pending::Kind::Delete, index, kNoIndex, {}};
  if (context.session.document().node(index).parent != kNoIndex &&
      ImGui::MenuItem(ICON_FA_ARROW_TURN_UP "  Move to top level")) {
    pending = {Pending::Kind::Reparent, index, kNoIndex, {}};
  }
  ImGui::Separator();
  if (ImGui::BeginMenu(ICON_FA_PLUS "  Create")) {
    draw_create_menu(context);
    ImGui::EndMenu();
  }
  ImGui::EndDisabled();
  ImGui::EndPopup();
}

// One row: tree node with the type icon, the name and a gem when it has scripts.
bool draw_row(PanelContext& context, int index, ImGuiTreeNodeFlags flags, HierarchyResult& result,
              Pending& pending) {
  EditorSession& session = context.session;
  // Copies: the context menu can create objects, which reallocates the node list.
  const NodeDesc& node_ref = session.document().node(index);
  const std::string name = node_ref.name;
  const char* icon = icon_of(node_ref);
  const bool template_object = get_bool(node_ref.props, config::kPropPrefab).value_or(false);
  const bool hidden = !get_bool(node_ref.props, config::kPropVisible).value_or(true);
  const bool has_script = get_string(node_ref.props, config::kPropScript).has_value();
  if (session.selection() == index) flags |= ImGuiTreeNodeFlags_Selected;
  flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth;

  const bool open = ImGui::TreeNodeEx("##node", flags);
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
    session.select(index);
  }
  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) result.frame_requested = true;
  if (session.play_state() == PlayState::Editing && ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload(kPayloadNode, &index, sizeof(index));
    ImGui::Text("%s  %s", icon, name.c_str());
    ImGui::EndDragDropSource();
  }
  accept_drops(index, pending);
  node_context_menu(context, index, pending);

  ImGui::SameLine();
  ImGui::TextColored(template_object ? accent_color() : muted_color(), "%s", icon);
  ImGui::SameLine();
  if (hidden || template_object) {
    ImGui::TextColored(muted_color(), "%s", name.c_str());
  } else {
    ImGui::TextUnformatted(name.c_str());
  }
  if (has_script) {
    ImGui::SameLine();
    ImGui::TextColored(accent_color(), ICON_FA_GEM);
  }
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

void apply(PanelContext& context, const Pending& pending) {
  EditorSession& session = context.session;
  switch (pending.kind) {
    case Pending::Kind::None: break;
    case Pending::Kind::Duplicate:
      session.select(pending.target);
      session.duplicate_selection();
      break;
    case Pending::Kind::Delete:
      session.select(pending.target);
      session.delete_selection();
      break;
    case Pending::Kind::Reparent: session.reparent(pending.target, pending.parent); break;
    case Pending::Kind::AttachScript:
      session.attach_script(pending.target, pending.file);
      session.select(pending.target);
      break;
    case Pending::Kind::ImportModel: {
      std::string error;
      const std::filesystem::path model = session.game_dir() / config::kModelsDirName / pending.file;
      if (session.import_model(model, context.spawn_point, error, pending.parent) == kNoObject) {
        log::error("cannot import " + pending.file + ": " + error);
      }
      break;
    }
  }
}

}  // namespace

HierarchyResult draw_hierarchy(PanelContext& context) {
  HierarchyResult result;
  EditorSession& session = context.session;
  ImGui::Begin(kHierarchyWindow);

  std::string& filter = g_filter;
  const bool editing = session.play_state() == PlayState::Editing;
  ImGui::BeginDisabled(!editing);
  if (icon_button(ICON_FA_PLUS, "Create an object")) ImGui::OpenPopup("create");
  ImGui::EndDisabled();
  if (ImGui::BeginPopup("create")) {
    draw_create_menu(context);
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint("##search", ICON_FA_MAGNIFYING_GLASS "  Search", &filter);
  ImGui::Separator();

  Pending pending;
  ImGui::BeginChild("tree");
  const SceneDocument& doc = session.document();
  if (filter.empty()) {
    for (int i = 0; i < doc.node_count(); ++i) {
      if (doc.node(i).parent == kNoIndex) draw_tree(context, i, result, pending);
    }
  } else {
    const std::string wanted = lowercase(filter);
    for (int i = 0; i < doc.node_count(); ++i) {
      if (lowercase(doc.node(i).name).find(wanted) == std::string::npos) continue;
      ImGui::PushID(i);
      draw_row(context, i, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, result, pending);
      ImGui::PopID();
    }
  }
  // The empty space below the tree: drop here to move objects to the top level, click to deselect.
  const ImVec2 free = ImGui::GetContentRegionAvail();
  if (ImGui::InvisibleButton("##empty", ImVec2(std::max(free.x, kRootDropMinWidth), std::max(free.y, kRootDropMinHeight)))) {
    session.select(kNoObject);
  }
  accept_drops(kNoIndex, pending);
  if (ImGui::BeginPopupContextItem("empty_menu")) {
    ImGui::BeginDisabled(!editing);
    draw_create_menu(context);
    ImGui::EndDisabled();
    ImGui::EndPopup();
  }
  ImGui::EndChild();
  ImGui::End();
  apply(context, pending);
  return result;
}

void reset_hierarchy_state() {
  g_filter.clear();
}

}  // namespace redgem::ui
