#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/script_files.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

using namespace editor_config;

constexpr float kFullVolume = 1.0f;            // sound previews play as recorded
constexpr float kNormalPitch = 1.0f;
constexpr float kTileLabelPadding = 4.0f;      // pixels between a tile's edge and its icon and name
constexpr float kNewScriptFieldWidth = 180.0f; // pixels
constexpr const char* kNewScriptPopup = "new_script";
constexpr const char* kFoldersId = "folders";
constexpr const char* kFilesId = "files";
constexpr const char* kTileId = "##tile";
constexpr const char* kNewScriptNameId = "##name";

enum class Folder { Scripts, Models, Sounds, Scenes };

struct FolderInfo {
  Folder folder;
  const char* icon;
  const char* title;
  const char* dir;
  const char* hint;  // shown when the folder is empty
};

constexpr FolderInfo kFolders[] = {
    {Folder::Scripts, ICON_FA_GEM, "Scripts", config::kScriptsDirName,
     "Ruby behaviours. Create one with New script, then drag it onto an object."},
    {Folder::Models, ICON_FA_CUBES, "Models", config::kModelsDirName,
     "Export .glb files from Blender into models/, then drag them into the scene."},
    {Folder::Sounds, ICON_FA_VOLUME_HIGH, "Sounds", config::kSoundsDirName,
     "Put .wav / .ogg / .mp3 / .flac files in sounds/ and play them with Audio.play(\"name\")."},
    {Folder::Scenes, ICON_FA_MAP, "Scenes", config::kScenesDirName,
     "Extra levels: scenes/<name>.glb, loaded with Game.load_scene(\"name\")."},
};

// State kept between frames. Reset when another game is opened (see reset_project_state).
std::string g_new_script_name;
Folder g_folder = Folder::Scripts;

const std::vector<std::string>& files_of(const ProjectFiles& files, Folder folder) {
  switch (folder) {
    case Folder::Models: return files.models;
    case Folder::Sounds: return files.sounds;
    case Folder::Scenes: return files.scenes;
    case Folder::Scripts: break;
  }
  return files.scripts;
}

// A file tile: big icon, name below. Returns true on double click.
bool tile(const char* icon, const std::string& name, const char* payload_type) {
  ImGui::PushID(name.c_str());
  const ImVec2 start = ImGui::GetCursorScreenPos();
  ImGui::Selectable(kTileId, false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(kTileWidth, kTileHeight));
  const bool opened = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  if (payload_type != nullptr && ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload(payload_type, name.c_str(), name.size() + 1);
    ImGui::Text("%s  %s", icon, name.c_str());
    ImGui::EndDragDropSource();
  }
  tooltip(name.c_str());

  ImDrawList* draw = ImGui::GetWindowDrawList();
  ImFont* font = ImGui::GetFont();
  const float icon_size = ImGui::GetFontSize() * kTileIconScale;
  const ImVec2 icon_extent = font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
  const ImVec2 icon_pos(start.x + editor_logic::centered_start(kTileWidth, icon_extent.x), start.y + kTileLabelPadding);
  draw->AddText(font, icon_size, icon_pos, ImGui::GetColorU32(muted_color()), icon);
  // The name, centered and clipped to the tile.
  const float text_y = start.y + kTileHeight - ImGui::GetTextLineHeight() - kTileLabelPadding;
  const ImVec2 name_size = ImGui::CalcTextSize(name.c_str());
  const float text_x = start.x + std::max(kTileLabelPadding, editor_logic::centered_start(kTileWidth, name_size.x));
  const ImVec4 clip(start.x + kTileLabelPadding, start.y, start.x + kTileWidth - kTileLabelPadding,
                    start.y + kTileHeight);
  draw->AddText(font, ImGui::GetFontSize(), ImVec2(text_x, text_y), ImGui::GetColorU32(ImGuiCol_Text),
                name.c_str(), nullptr, 0.0f, &clip);
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
  std::string& name = g_new_script_name;
  if (!ImGui::BeginPopup(kNewScriptPopup)) return;
  ImGui::TextUnformatted("New Ruby behaviour");
  ImGui::SetNextItemWidth(kNewScriptFieldWidth);
  if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
  const bool entered =
      ImGui::InputTextWithHint(kNewScriptNameId, "enemy_ai", &name, ImGuiInputTextFlags_EnterReturnsTrue);
  const bool valid = valid_script_name(name);
  ImGui::SameLine();
  ImGui::BeginDisabled(!valid);
  if (ImGui::Button("Create") || (entered && valid)) create_and_open_script(context, name);
  ImGui::EndDisabled();
  if (!name.empty() && !valid) ImGui::TextColored(muted_color(), "%s", kScriptNameRule);
  ImGui::EndPopup();
}

void open_file(PanelContext& context, Folder folder, const std::string& name) {
  EditorSession& session = context.session;
  switch (folder) {
    case Folder::Scripts: open_script(context.scripts, name); break;
    case Folder::Models: import_model(session, name, context.spawn_point); break;
    case Folder::Sounds:
      if (context.audio != nullptr) {
        context.audio->set_sounds_dir(session.game_dir() / config::kSoundsDirName);
        context.audio->play_sound(name, kFullVolume, kNormalPitch);
      }
      break;
    case Folder::Scenes:
      log::info("load it from a script: Game.load_scene(\"" + name + "\")");
      break;
  }
}

const FolderInfo& info_of(Folder folder) {
  return *std::find_if(std::begin(kFolders), std::end(kFolders),
                       [folder](const FolderInfo& info) { return info.folder == folder; });
}

// What can be dragged out of a folder's tiles (nullptr: nothing).
const char* payload_of(Folder folder) {
  if (folder == Folder::Scripts) return kPayloadScript;
  return folder == Folder::Models ? kPayloadModel : nullptr;
}

// The list of folders on the left, each with how many files it holds.
void draw_folders(const ProjectFiles& files) {
  ImGui::BeginChild(kFoldersId, ImVec2(kFolderColumnWidth, 0.0f), ImGuiChildFlags_ResizeX);
  for (const FolderInfo& info : kFolders) {
    const std::string label = std::string(info.icon) + "  " + info.title + "  (" +
                              std::to_string(files_of(files, info.folder).size()) + ")";
    if (ImGui::Selectable(label.c_str(), g_folder == info.folder)) g_folder = info.folder;
  }
  ImGui::EndChild();
}

// The current folder's path and its buttons.
void draw_folder_bar(PanelContext& context, const FolderInfo& info) {
  const std::filesystem::path game_dir = context.session.game_dir();
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(muted_color(), "%s / %s", game_dir.filename().string().c_str(), info.dir);
  ImGui::SameLine();
  if (info.folder == Folder::Scripts) {
    if (ImGui::SmallButton(ICON_FA_PLUS "  New script")) ImGui::OpenPopup(kNewScriptPopup);
    new_script_popup(context);
    ImGui::SameLine();
  }
  if (!ImGui::SmallButton(ICON_FA_FOLDER_OPEN "  Open folder")) return;
  std::error_code fs_error;
  std::filesystem::create_directories(game_dir / info.dir, fs_error);
  open_in_system(game_dir / info.dir);
}

// The current folder's files as a grid of tiles.
void draw_tiles(PanelContext& context, const FolderInfo& info) {
  ImGui::BeginChild(kFilesId);
  const std::vector<std::string>& names = files_of(context.files, info.folder);
  if (names.empty()) ImGui::TextColored(muted_color(), "%s", info.hint);
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const int columns = editor_logic::tile_columns(ImGui::GetContentRegionAvail().x, spacing, kTileWidth);
  int column = 0;
  for (const std::string& name : names) {
    if (column++ % columns != 0) ImGui::SameLine();
    if (tile(info.icon, name, payload_of(info.folder))) open_file(context, info.folder, name);
  }
  ImGui::EndChild();
}

}  // namespace

void draw_project(PanelContext& context) {
  ImGui::Begin(kProjectWindow);
  draw_folders(context.files);
  ImGui::SameLine();
  const FolderInfo& info = info_of(g_folder);
  ImGui::BeginGroup();
  draw_folder_bar(context, info);
  draw_tiles(context, info);
  ImGui::EndGroup();
  ImGui::End();
}

void reset_project_state() {
  g_new_script_name.clear();
  g_folder = Folder::Scripts;
}

}  // namespace redgem::ui
