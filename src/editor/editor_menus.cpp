// Part of EditorApp: the menu bar.

#include "editor/editor_app.hpp"

#include <imgui.h>

#include "app/user_settings.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr const char* kNoBuilds = "editor.menu.no_builds";

// Saved for this user (not in the game): which card draws the editor and the games it starts.
struct GpuOption {
  GpuChoice choice;
  const char* label;  // the key of its text
};
constexpr GpuOption kGpuOptions[] = {
    {GpuChoice::Auto, "editor.gpu.automatic"},
    {GpuChoice::Dedicated, "editor.gpu.dedicated"},
    {GpuChoice::Integrated, "editor.gpu.integrated"},
};

void choose_gpu(UserSettings settings, GpuChoice choice) {
  settings.gpu = choice;
  std::string error;
  if (write_user_settings(user_settings_file(), settings, error)) {
    log::info(std::string("graphics card setting: ") + gpu_choice_name(choice) +
              " (used the next time the editor or a game starts)");
  } else {
    log::error("cannot save the setting: " + error);
  }
}

}  // namespace

void EditorApp::draw_menu_bar() {
  if (!ImGui::BeginMainMenuBar()) return;
  ImGui::TextColored(ui::accent_color(), ICON_FA_GEM);
  ui::menu(tr("editor.menu.file"), [this] { draw_file_menu(); });
  ui::menu(tr("editor.menu.edit"), [this] { draw_edit_menu(); });
  ui::menu(tr("editor.menu.view"), [this] { draw_view_menu(); });
  ui::menu(tr("editor.menu.game_object"), [this] { draw_game_object_menu(); }, editing());
  ui::menu(tr("editor.menu.help"), [this] { draw_help_menu(); });
  ImGui::EndMainMenuBar();
}

void EditorApp::draw_game_object_menu() {
  ui::PanelContext context = panel_context();
  ui::draw_create_menu(context);
}

void EditorApp::draw_help_menu() {
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_KEYBOARD, "editor.menu.keyboard_shortcuts"))) shortcuts_open_ = true;
}

void EditorApp::draw_file_menu() {
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_FILE_CIRCLE_PLUS, "editor.menu.new_game")))
    request_exit(EditorExit::OpenHub);
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_FOLDER_OPEN, "editor.menu.open_game"))) request_exit(EditorExit::OpenHub);
  ImGui::Separator();
  draw_game_file_items();
  ImGui::Separator();
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_RIGHT_FROM_BRACKET, "editor.menu.quit"))) request_exit(EditorExit::Quit);
}

// What the File menu offers for the game being edited.
void EditorApp::draw_game_file_items() {
  draw_scene_items();
  ImGui::Separator();
  ui::menu(ui::icon_label(ICON_FA_BOX_ARCHIVE, "editor.menu.build_game"), [this] { draw_build_targets(); }, editing());
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_FOLDER, "editor.menu.show_game_folder")))
    platform::open_in_system(session_->game_dir());
}

// The File menu's scene commands: what the open scene is saved as, and which scene is open.
void EditorApp::draw_scene_items() {
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_FLOPPY_DISK, "editor.menu.save_scene"), tr(kKeysSave), false, editing()))
    save();
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_COPY, "editor.menu.save_scene_as"), nullptr, false, editing()))
    ask_scene_name(SceneNameDialog::SaveAs);
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_MAP, "editor.menu.new_scene"), nullptr, false, editing()))
    ask_scene_name(SceneNameDialog::NewScene);
  ui::menu(
      ui::icon_label(ICON_FA_FOLDER_OPEN, "editor.menu.open_scene"), [this] { draw_open_scene_items(); }, editing());
}

// Every scene of the game, the open one checked.
void EditorApp::draw_open_scene_items() {
  const std::string open = session_->scene_name();
  for (const std::string& scene : files_.scenes) {
    if (ImGui::MenuItem(scene.c_str(), nullptr, scene == open)) open_scene(scene);
  }
}

void EditorApp::draw_build_targets() {
  if (builds_.empty()) ImGui::TextColored(ui::muted_color(), "%s", tr(kNoBuilds));
  for (const BuildChoice& choice : builds_) {
    if (ImGui::MenuItem(choice.label.c_str())) build(choice);
  }
}

void EditorApp::draw_edit_menu() {
  draw_undo_items();
  ImGui::Separator();
  draw_selection_items();
  ImGui::Separator();
  ui::menu(ui::icon_label(ICON_FA_MICROCHIP, "editor.menu.graphics_card"), [this] { draw_graphics_card_menu(); });
}

void EditorApp::draw_undo_items() {
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_ROTATE_LEFT, "editor.menu.undo"), tr(kKeysUndo), false, undo_offered()))
    session_->undo();
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_ROTATE_RIGHT, "editor.menu.redo"), tr(kKeysRedo), false, redo_offered()))
    session_->redo();
}

bool EditorApp::undo_offered() const { return editing() && session_->document().can_undo(); }
bool EditorApp::redo_offered() const { return editing() && session_->document().can_redo(); }

void EditorApp::draw_selection_items() {
  const bool has_selection = session_->selection() != kNoObject;
  draw_clipboard_items();
  if (ImGui::MenuItem(ui::duplicate_label(), tr(kKeysDuplicate), false, selection_editable())) {
    session_->duplicate_selection();
  }
  if (ImGui::MenuItem(ui::delete_label(), tr(kKeysDelete), false, selection_editable())) session_->delete_selection();
  ImGui::Separator();
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_EXPAND, "editor.menu.frame_selection"), tr(kKeysFrame), false,
                      has_selection))
    frame_selection();
}

void EditorApp::draw_clipboard_items() {
  if (ImGui::MenuItem(ui::cut_label(), tr(kKeysCut), false, selection_editable())) cut_selection();
  if (ImGui::MenuItem(ui::copy_label(), tr(kKeysCopy), false, selection_editable())) copy_selection();
  if (ImGui::MenuItem(ui::paste_label(), tr(kKeysPaste), false, editing())) paste();
  if (ImGui::MenuItem(ui::paste_into_label(), tr(kKeysPasteInto), false, selection_editable())) {
    paste_into(session_->selection());
  }
  ImGui::Separator();
}

bool EditorApp::selection_editable() const { return editing() && session_->selection() != kNoObject; }

void EditorApp::draw_view_menu() {
  ImGui::MenuItem(ui::icon_label(ICON_FA_LIST, "editor.menu.hierarchy_panel"), tr(kKeysHierarchy), &panels_.hierarchy);
  ImGui::MenuItem(ui::icon_label(ICON_FA_SLIDERS, "editor.menu.inspector_panel"), tr(kKeysInspector),
                  &panels_.inspector);
  ImGui::MenuItem(ui::icon_label(ICON_FA_TERMINAL, "editor.menu.bottom_panels"), tr(kKeysBottomPanel), &panels_.bottom);
  ImGui::MenuItem(ui::icon_label(ICON_FA_ROBOT, "editor.menu.agent_panel"), nullptr, &panels_.agent);
  ImGui::Separator();
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_CODE, "editor.menu.focus_on_code"))) panels_ = {false, false, false, false};
  if (ImGui::MenuItem(ui::icon_label(ICON_FA_TABLE_COLUMNS, "editor.menu.show_all_panels"))) panels_ = {};
}

void EditorApp::draw_graphics_card_menu() {
  const UserSettings settings = read_user_settings(user_settings_file());
  for (const GpuOption& option : kGpuOptions) {
    if (ImGui::MenuItem(tr(option.label), nullptr, settings.gpu == option.choice)) choose_gpu(settings, option.choice);
  }
  ImGui::Separator();
  ImGui::TextColored(ui::muted_color(), "%s", tr("editor.gpu.next_start"));
}

}  // namespace redgem
