// Part of EditorApp: the toolbar under the menu bar, and the status bar at the bottom.

#include "editor/editor_app.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <raylib.h>

#include <string>

#include "core/filled_texts.hpp"
#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr float kRightButtons = 2.0f;  // Build and the Inspector toggle
constexpr float kPlayControls = 2.0f;  // Play/Stop and Pause, centered together
constexpr const char* kToolbarId = "##toolbar";
constexpr const char* kStatusBarId = "##status";
constexpr const char* kBuildPopup = "build_target";

// A bar across the top or the bottom of the window, in the menu bar's color. ImGui::End() closes
// it whatever this returns.
bool begin_bar(const char* id, ImGuiDir side, float height) {
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
  const bool open = ImGui::BeginViewportSideBar(id, ImGui::GetMainViewport(), side, height, flags);
  ImGui::PopStyleColor();
  return open;
}

// The strip under the menu bar. Like begin_bar, ImGui::End() closes it whatever this returns.
bool begin_toolbar() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  const bool open = begin_bar(kToolbarId, ImGuiDir_Up, kToolbarHeight);
  ImGui::PopStyleVar();
  return open;
}

bool begin_status_bar() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetStyle().WindowPadding.x, 0.0f));
  const bool open = begin_bar(kStatusBarId, ImGuiDir_Down, kStatusBarHeight);
  ImGui::PopStyleVar();
  return open;
}

float icon_button_width() { return ImGui::GetFrameHeight() * kIconButtonWidth; }

ImVec4 level_color(log::Level level) {
  if (level == log::Level::Error) return ui::error_color();
  return level == log::Level::Warning ? ui::warning_color() : ui::muted_color();
}

const char* play_state_label(PlayState state) {
  if (state == PlayState::Playing) return ui::icon_label(ICON_FA_PLAY, "editor.status.playing");
  return state == PlayState::Paused ? ui::icon_label(ICON_FA_PAUSE, "editor.status.paused")
                                    : ui::icon_label(ICON_FA_PEN, "editor.status.editing");
}

}  // namespace

void EditorApp::draw_toolbar() {
  if (begin_toolbar()) {
    ui::toggle_button(ICON_FA_TABLE_LIST, tr("editor.toolbar.hierarchy"), panels_.hierarchy);
    ImGui::SameLine(0.0f, kToolbarGroupGap);
    ImGui::BeginDisabled(!editing());
    gizmo_.draw_tools();
    ImGui::EndDisabled();
    draw_play_controls();
    draw_game_tools();
  }
  ImGui::End();
}

// Play while editing, Stop while the game runs.
void EditorApp::draw_play_button(bool edit) {
  if (edit) {
    if (ui::icon_button(ICON_FA_PLAY, tr("editor.toolbar.play"))) play();
    return;
  }
  ImGui::PushStyleColor(ImGuiCol_Text, ui::accent_color());
  if (ui::icon_button(ICON_FA_STOP, tr("editor.toolbar.stop"), true)) stop();
  ImGui::PopStyleColor();
}

// Center of the toolbar: Play / Stop and Pause.
void EditorApp::draw_play_controls() {
  const bool edit = editing();
  const float group = icon_button_width() * kPlayControls + ImGui::GetStyle().ItemSpacing.x;
  ImGui::SameLine(editor_logic::centered_start(ImGui::GetWindowContentRegionMax().x, group));
  draw_play_button(edit);
  ImGui::SameLine();
  ImGui::BeginDisabled(edit);
  if (ui::icon_button(ICON_FA_PAUSE, tr("editor.toolbar.pause"), session_->play_state() == PlayState::Paused))
    toggle_pause();
  ImGui::EndDisabled();
}

// Right of the toolbar: the game's name, Build and the Inspector toggle.
void EditorApp::draw_game_tools() {
  draw_game_name();
  ImGui::SameLine();
  draw_build_button();
  ImGui::SameLine(0.0f, kToolbarGroupGap);
  ui::toggle_button(ICON_FA_TABLE_LIST, tr("editor.toolbar.inspector"), panels_.inspector);
}

// The game's name (with a dot while the scene has unsaved changes), placed so that the buttons
// after it end at the right edge.
void EditorApp::draw_game_name() {
  const std::string game = session_->game_dir().filename().string();
  const std::string label =
      std::string(ICON_FA_GAMEPAD "  ") + game + (session_->document().dirty() ? "  " ICON_FA_CIRCLE : "");
  const float width = ImGui::CalcTextSize(label.c_str()).x;
  const float buttons = (icon_button_width() + ImGui::GetStyle().ItemSpacing.x) * kRightButtons;
  ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - width - buttons - kToolbarGroupGap);
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(ui::muted_color(), "%s", label.c_str());
}

void EditorApp::draw_build_button() {
  ImGui::BeginDisabled(!editing());
  if (ui::icon_button(ICON_FA_BOX_ARCHIVE, tr("editor.toolbar.build"))) {
    ImGui::OpenPopup(kBuildPopup);
  }
  ui::popup(kBuildPopup, [this] { draw_build_targets(); });
  ImGui::EndDisabled();
}

void EditorApp::draw_status_bar() {
  if (begin_status_bar()) {
    ImGui::SetCursorPosY(editor_logic::centered_start(kStatusBarHeight, ImGui::GetTextLineHeight()));
    draw_last_message();
    draw_scene_summary();
  }
  ImGui::End();
}

// Left of the status bar: the console's newest line.
void EditorApp::draw_last_message() {
  if (console_->entries().empty()) return;
  const ConsoleEntry& last = console_->entries().back();
  ImGui::TextColored(level_color(last.level), "%s", last.message.c_str());
}

// Right of the status bar: the selection, the open scene and how big it is, the play state and the
// frame rate.
void EditorApp::draw_scene_summary() {
  static FilledTexts objects("editor.status.objects", "count");
  static FilledTexts fps("editor.status.fps", "fps");
  const ObjectId selected = session_->selection();
  char info[kTextBufferSize];
  ImFormatString(info, sizeof(info), "%s    %s %s    %s %s    %s    %s",
                 selected != kNoObject ? session_->document().node(selected).name.c_str() : "", ICON_FA_MAP,
                 session_->scene_name().c_str(), ICON_FA_CUBES, objects.with(session_->document().node_count()),
                 play_state_label(session_->play_state()), fps.with(GetFPS()));
  ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(info).x);
  ImGui::TextColored(ui::muted_color(), "%s", info);
}

}  // namespace redgem
