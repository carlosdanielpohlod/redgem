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

constexpr const char* kNoBuilds = "No way of building was given to this editor.";

// Saved for this user (not in the game): which card draws the editor and the games it starts.
struct GpuOption {
  GpuChoice choice;
  const char* label;
};
constexpr GpuOption kGpuOptions[] = {
    {GpuChoice::Auto, "Automatic (dedicated when there is one)"},
    {GpuChoice::Dedicated, "Dedicated (faster)"},
    {GpuChoice::Integrated, "Integrated (cooler and quieter, much slower)"},
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
  if (ImGui::MenuItem(ICON_FA_KEYBOARD "  Keyboard Shortcuts")) shortcuts_open_ = true;
}

void EditorApp::draw_file_menu() {
  if (ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS "  New Game...")) request_exit(EditorExit::OpenHub);
  if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Open Game...")) request_exit(EditorExit::OpenHub);
  ImGui::Separator();
  draw_game_file_items();
  ImGui::Separator();
  if (ImGui::MenuItem(ICON_FA_RIGHT_FROM_BRACKET "  Quit")) request_exit(EditorExit::Quit);
}

// What the File menu offers for the game being edited.
void EditorApp::draw_game_file_items() {
  if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save Scene", kKeysSave, false, editing())) save();
  ui::menu(ICON_FA_BOX_ARCHIVE "  Build Game", [this] { draw_build_targets(); }, editing());
  if (ImGui::MenuItem(ICON_FA_FOLDER "  Show Game Folder")) platform::open_in_system(session_->game_dir());
}

void EditorApp::draw_build_targets() {
  if (builds_.empty()) ImGui::TextColored(ui::muted_color(), "%s", kNoBuilds);
  for (const BuildChoice& choice : builds_) {
    if (ImGui::MenuItem(choice.label.c_str())) build(choice);
  }
}

void EditorApp::draw_edit_menu() {
  draw_undo_items();
  ImGui::Separator();
  draw_selection_items();
  ImGui::Separator();
  ui::menu(ICON_FA_MICROCHIP "  Graphics Card", [this] { draw_graphics_card_menu(); });
}

void EditorApp::draw_undo_items() {
  if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  Undo", kKeysUndo, false, undo_offered())) session_->undo();
  if (ImGui::MenuItem(ICON_FA_ROTATE_RIGHT "  Redo", kKeysRedo, false, redo_offered())) session_->redo();
}

bool EditorApp::undo_offered() const { return editing() && session_->document().can_undo(); }
bool EditorApp::redo_offered() const { return editing() && session_->document().can_redo(); }

void EditorApp::draw_selection_items() {
  const bool has_selection = session_->selection() != kNoObject;
  if (ImGui::MenuItem(ui::duplicate_label(), kKeysDuplicate, false, selection_editable())) {
    session_->duplicate_selection();
  }
  if (ImGui::MenuItem(ui::delete_label(), kKeysDelete, false, selection_editable())) session_->delete_selection();
  ImGui::Separator();
  if (ImGui::MenuItem(ICON_FA_EXPAND "  Frame Selection", kKeysFrame, false, has_selection)) frame_selection();
}

bool EditorApp::selection_editable() const { return editing() && session_->selection() != kNoObject; }

void EditorApp::draw_view_menu() {
  ImGui::MenuItem(ICON_FA_LIST "  Hierarchy (left)", kKeysHierarchy, &panels_.hierarchy);
  ImGui::MenuItem(ICON_FA_SLIDERS "  Inspector (right)", kKeysInspector, &panels_.inspector);
  ImGui::MenuItem(ICON_FA_TERMINAL "  Project and Console (bottom)", kKeysBottomPanel, &panels_.bottom);
  ImGui::Separator();
  if (ImGui::MenuItem(ICON_FA_CODE "  Focus on code and scene")) panels_ = {false, false, false};
  if (ImGui::MenuItem(ICON_FA_TABLE_COLUMNS "  Show all panels")) panels_ = {};
}

void EditorApp::draw_graphics_card_menu() {
  const UserSettings settings = read_user_settings(user_settings_file());
  for (const GpuOption& option : kGpuOptions) {
    if (ImGui::MenuItem(option.label, nullptr, settings.gpu == option.choice)) choose_gpu(settings, option.choice);
  }
  ImGui::Separator();
  ImGui::TextColored(ui::muted_color(), "Takes effect on the next start.");
}

}  // namespace redgem
