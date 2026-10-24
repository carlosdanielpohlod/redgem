#include <imgui.h>

#include <algorithm>

#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/scripts/script_files.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/project_folders.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

using namespace editor_config;

constexpr float kTileLabelPadding = 4.0f;       // pixels between a tile's edge and its icon and name
constexpr float kNewScriptFieldWidth = 180.0f;  // pixels
constexpr const char* kNewScriptPopup = "new_script";
constexpr const char* kFoldersId = "folders";
constexpr const char* kFilesId = "files";
constexpr const char* kTileId = "##tile";
constexpr const char* kNewScriptNameId = "##name";

using namespace project;
using Folder = ProjectFolder;

// The tile just drawn can be dragged out as `payload_type` (nullptr: it cannot).
void offer_tile_drag(const char* icon, const std::string& name, const char* payload_type) {
  if (payload_type == nullptr) return;
  if (!ImGui::BeginDragDropSource()) return;
  ImGui::SetDragDropPayload(payload_type, name.c_str(), name.size() + 1);
  ImGui::Text("%s  %s", icon, name.c_str());
  ImGui::EndDragDropSource();
}

// The big icon at the top of a tile that starts at `start`, centered.
void draw_tile_icon(ImVec2 start, const char* icon) {
  ImFont* font = ImGui::GetFont();
  const float icon_size = ImGui::GetFontSize() * kTileIconScale;
  const ImVec2 icon_extent = font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
  const ImVec2 icon_pos(start.x + editor_logic::centered_start(kTileWidth, icon_extent.x), start.y + kTileLabelPadding);
  ImGui::GetWindowDrawList()->AddText(font, icon_size, icon_pos, ImGui::GetColorU32(muted_color()), icon);
}

// The name at the bottom of a tile, centered and clipped to the tile.
void draw_tile_name(ImVec2 start, const std::string& name) {
  const float text_y = start.y + kTileHeight - ImGui::GetTextLineHeight() - kTileLabelPadding;
  const ImVec2 name_size = ImGui::CalcTextSize(name.c_str());
  const float text_x = start.x + std::max(kTileLabelPadding, editor_logic::centered_start(kTileWidth, name_size.x));
  const ImVec4 clip(start.x + kTileLabelPadding, start.y, start.x + kTileWidth - kTileLabelPadding,
                    start.y + kTileHeight);
  ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(text_x, text_y),
                                      ImGui::GetColorU32(ImGuiCol_Text), name.c_str(), nullptr, 0.0f, &clip);
}

bool double_clicked() { return ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left); }

// A file tile: big icon, name below. Returns true on double click.
bool tile(const char* icon, const std::string& name, const char* payload_type) {
  ImGui::PushID(name.c_str());
  const ImVec2 start = ImGui::GetCursorScreenPos();
  ImGui::Selectable(kTileId, false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(kTileWidth, kTileHeight));
  const bool opened = double_clicked();
  offer_tile_drag(icon, name, payload_type);
  tooltip(name.c_str());
  draw_tile_icon(start, icon);
  draw_tile_name(start, name);
  ImGui::PopID();
  return opened;
}

// Makes scripts/<name>.rb and opens it in a tab.
void create_and_open_script(PanelContext& context, std::string& name) {
  std::string error;
  const auto file = create_script(context.scripts.scripts_dir(), name, error);
  if (!file) {
    log::error(error);
    return;
  }
  log::info("created " + *file);
  open_script(context.scripts, *file);
  context.files_changed = true;
  name.clear();
  ImGui::CloseCurrentPopup();
}

void new_script_popup(PanelContext& context) {
  std::string& name = context.state.project.new_script_name;
  if (!ImGui::BeginPopup(kNewScriptPopup)) return;
  ImGui::TextUnformatted(tr("editor.project.new_script_title"));
  ImGui::SetNextItemWidth(kNewScriptFieldWidth);
  if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
  if (draw_new_script_form(kNewScriptNameId, tr("editor.project.new_script_hint"), tr("editor.project.create_script"),
                           name)) {
    create_and_open_script(context, name);
  }
  ImGui::EndPopup();
}

// The list of folders on the left, each with how many files it holds.
void draw_folders(const ProjectFiles& files, Folder& current) {
  ImGui::BeginChild(kFoldersId, ImVec2(kFolderColumnWidth, 0.0f), ImGuiChildFlags_ResizeX);
  for (const FolderInfo& info : folders()) {
    const std::string label = std::string(info.icon) + "  " + tr(info.title) + "  (" +
                              std::to_string(files_of(files, info.folder).size()) + ")";
    if (ImGui::Selectable(label.c_str(), current == info.folder)) current = info.folder;
  }
  ImGui::EndChild();
}

// What the folders that hold files made here offer to make one: New script, New scene.
void draw_new_file_button(PanelContext& context, Folder folder) {
  if (folder == Folder::Scripts) {
    if (ImGui::SmallButton(icon_label(ICON_FA_PLUS, "editor.project.new_script"))) ImGui::OpenPopup(kNewScriptPopup);
    new_script_popup(context);
    ImGui::SameLine();
  }
  if (folder == Folder::Scenes) {
    ImGui::BeginDisabled(!context.session.editing());  // like File > New Scene: not while the game runs
    context.new_scene_asked = ImGui::SmallButton(icon_label(ICON_FA_PLUS, "editor.project.new_scene"));
    ImGui::EndDisabled();
    ImGui::SameLine();
  }
}

// The current folder's path and its buttons.
void draw_folder_bar(PanelContext& context, const FolderInfo& info) {
  const std::filesystem::path game_dir = context.session.game_dir();
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(muted_color(), "%s / %s", game_dir.filename().string().c_str(), info.dir);
  ImGui::SameLine();
  draw_new_file_button(context, info.folder);
  if (!ImGui::SmallButton(icon_label(ICON_FA_FOLDER_OPEN, "editor.project.open_folder"))) return;
  std::error_code fs_error;
  std::filesystem::create_directories(game_dir / info.dir, fs_error);
  platform::open_in_system(game_dir / info.dir);
}

// The current folder's files as a grid of tiles.
void draw_tiles(PanelContext& context, const FolderInfo& info) {
  ImGui::BeginChild(kFilesId);
  const std::vector<std::string>& names = files_of(context.files, info.folder);
  if (names.empty()) ImGui::TextColored(muted_color(), "%s", tr(info.hint));
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const int columns = editor_logic::tile_columns(ImGui::GetContentRegionAvail().x, spacing, kTileWidth);
  int column = 0;
  for (const std::string& name : names) {
    if (column++ % columns != 0) ImGui::SameLine();
    if (tile(info.icon, name, payload_of(info.folder))) info.open(context, name);
  }
  ImGui::EndChild();
}

}  // namespace

void draw_project(PanelContext& context) {
  ImGui::Begin(window_title(kProjectTitle, kProjectWindow));
  draw_folders(context.files, context.state.project.folder);
  ImGui::SameLine();
  const FolderInfo& info = info_of(context.state.project.folder);
  ImGui::BeginGroup();
  draw_folder_bar(context, info);
  draw_tiles(context, info);
  ImGui::EndGroup();
  ImGui::End();
}

}  // namespace redgem::ui
