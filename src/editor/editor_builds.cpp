// Part of EditorApp: building the game in the background (see build_job.hpp), the window that shows
// how a build goes and how it ended, and the dialog that waits for one before the editor closes.

#include "editor/editor_app.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <string>

#include "core/filled_texts.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/scene_viewport.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr const char* kBuildStatusWindow = "##build_status";
constexpr float kBuildStatusWidth = 380.0f;  // pixels: room for a folder's path on two lines
constexpr float kBottomRight = 1.0f;         // the window's pivot: it hangs from its bottom-right corner
constexpr ImGuiWindowFlags kBuildStatusFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                               ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
const char* build_wait_popup() { return ui::window_title("editor.build.wait_title", "###build_wait"); }

// In the Scene view's bottom-right corner (its hint takes the bottom-left one), as wide as a path
// on two lines needs.
void place_build_status(const SceneViewport& view) {
  const ImVec2 corner(view.screen_x + view.width_pixels() - kOverlayMargin,
                      view.screen_y + view.height_pixels() - kOverlayMargin);
  ImGui::SetNextWindowPos(corner, ImGuiCond_Always, ImVec2(kBottomRight, kBottomRight));
  ImGui::SetNextWindowSizeConstraints(ImVec2(kBuildStatusWidth, 0.0f), ImVec2(kBuildStatusWidth, FLT_MAX));
}

// What the build said it is doing, as a line of its own ("Copying the game's files").
std::string step_text(const std::string& step) {
  return step.empty() ? std::string(tr("editor.build.starting")) : editor_logic::capitalized(step);
}

void draw_result_headline(const BuildOutcome& outcome) {
  const TextValues values = {{"target", outcome.target}, {"time", elapsed_text(outcome.seconds)}};
  if (outcome.succeeded()) {
    ImGui::TextColored(ui::success_color(), ICON_FA_CIRCLE_CHECK);
    ImGui::SameLine();
    ImGui::TextUnformatted(tr("editor.build.built", values).c_str());
    return;
  }
  ImGui::TextColored(ui::error_color(), ICON_FA_CIRCLE_XMARK);
  ImGui::SameLine();
  ImGui::TextUnformatted(tr("editor.build.failed", values).c_str());
}

// What the build logged, then how it ended, into the console (on the editor's thread).
void report(const BuildOutcome& outcome) {
  for (const BuildMessage& message : outcome.messages) log::write(message.level, message.text);
  if (outcome.made) {
    log::info("built for " + outcome.target + ": " + outcome.made->string());
    return;
  }
  log::error("build failed: " + outcome.error);
}

// The folder a build filled; nullopt when it failed.
std::optional<std::filesystem::path> filled_folder(const BuildOutcome& outcome) {
  if (!outcome.succeeded()) return std::nullopt;
  return outcome.out_dir;
}

}  // namespace

bool EditorApp::start_build(const BuildChoice& choice) {
  if (building()) {
    log::error("cannot build for " + choice.name + ": the build for " + build_job_.target() + " is still running");
    return false;
  }
  if (!saved_for_build()) return false;
  last_build_.reset();
  if (!build_job_.start({choice, session_->game_dir(), build_folder(choice)})) {
    log::error("cannot build for " + choice.name + ": no thread could be started for it");
    return false;
  }
  log::info("building for " + choice.name + "...");
  return true;
}

// (The build reads the game from disk: what is saved when it starts is what it ships.)
bool EditorApp::saved_for_build() {
  if (session_->document().dirty()) save();
  save_scripts();
  if (!unsaved_work()) return true;
  log::error("cannot build: the game's edits could not be saved");
  return false;
}

std::optional<std::filesystem::path> EditorApp::build(const BuildChoice& choice) {
  if (!start_build(choice)) return std::nullopt;
  build_job_.wait();
  return take_build_outcome();
}

std::optional<std::filesystem::path> EditorApp::build() {
  const auto own =
      std::find_if(builds_.begin(), builds_.end(), [](const BuildChoice& choice) { return choice.for_this_system; });
  if (own != builds_.end()) return build(*own);
  log::error("build failed: this editor was not given a way to build for this system");
  return std::nullopt;
}

std::filesystem::path EditorApp::build_folder(const BuildChoice& choice) const {
  const std::filesystem::path game = session_->game_dir();
  return game.parent_path() /
         editor_logic::build_folder_name(game.filename().string(), choice.name, choice.for_this_system);
}

std::optional<std::filesystem::path> EditorApp::take_build_outcome() {
  std::optional<BuildOutcome> outcome = build_job_.take_outcome();
  if (!outcome) return std::nullopt;
  report(*outcome);
  std::optional<std::filesystem::path> filled = filled_folder(*outcome);
  last_build_ = std::move(outcome);
  leave_if_asked();
  return filled;
}

void EditorApp::leave_if_asked() {
  if (!leave_after_build_) return;
  leave_after_build_ = false;
  request_exit(exit_action_);
}

void EditorApp::draw_build_status() {
  if (!building() && !last_build_) return;
  place_build_status(*viewport_);
  if (ImGui::Begin(kBuildStatusWindow, nullptr, kBuildStatusFlags)) {
    if (building()) {
      draw_build_progress();
    } else {
      draw_build_result(*last_build_);
    }
  }
  ImGui::End();
}

// A turning circle, what is built and for how long it has been, the step it is at, and a bar that
// keeps moving (how long each step takes is not known).
void EditorApp::draw_build_progress() {
  static FilledTexts building_for("editor.build.building", "target");
  ui::spinner();
  ImGui::SameLine();
  ImGui::TextUnformatted(building_for.with(build_job_.target()));
  ImGui::SameLine();
  ImGui::TextColored(ui::muted_color(), "%s", elapsed_text(build_job_.seconds()).c_str());
  ImGui::TextColored(ui::muted_color(), "%s", step_text(build_job_.step()).c_str());
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ui::accent_color());
  ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()), ImVec2(kFillWidth, 0.0f), "");
  ImGui::PopStyleColor();
}

void EditorApp::draw_build_result(const BuildOutcome& outcome) {
  draw_result_headline(outcome);
  const std::string detail = outcome.succeeded() ? outcome.out_dir.string() : outcome.error;
  ImGui::PushStyleColor(ImGuiCol_Text, ui::muted_color());
  ImGui::TextWrapped("%s", detail.c_str());
  ImGui::PopStyleColor();
  draw_build_result_actions(outcome);
}

// Open folder (when there is one to open) and Close. (Closing forgets `outcome`: it is the last
// thing done with it.)
void EditorApp::draw_build_result_actions(const BuildOutcome& outcome) {
  if (outcome.succeeded()) {
    if (ImGui::Button(ui::icon_label(ICON_FA_FOLDER_OPEN, "editor.build.open_folder"))) {
      platform::open_in_system(outcome.out_dir);
    }
    ImGui::SameLine();
  }
  if (ImGui::Button(ui::icon_label(ICON_FA_XMARK, "editor.build.close"))) last_build_.reset();
}

// Leaving waits for the build to end, and says so; the user can stay instead.
void EditorApp::draw_build_wait_dialog() {
  if (build_wait_dialog_wanted()) ImGui::OpenPopup(build_wait_popup());
  if (!ImGui::BeginPopupModal(build_wait_popup(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  static FilledTexts waiting("editor.build.wait_question", "target");
  ui::spinner();
  ImGui::SameLine();
  ImGui::TextUnformatted(waiting.with(build_job_.target()));
  ImGui::TextColored(ui::muted_color(), "%s", step_text(build_job_.step()).c_str());
  if (ImGui::Button(tr("editor.build.keep_open"))) {
    leave_after_build_ = false;
    exit_action_ = EditorExit::Quit;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

// (The scene changed on disk is asked about first: one question at a time.)
bool EditorApp::build_wait_dialog_wanted() const {
  return waiting_for_build() && !session_->external_change_pending() && !ImGui::IsPopupOpen(build_wait_popup());
}

}  // namespace redgem
