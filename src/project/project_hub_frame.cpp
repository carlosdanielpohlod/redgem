// ProjectHub: one frame of its window. The title, and under it the two columns.

#include "project/project_hub.hpp"

#include <imgui.h>
#include <raylib.h>

#include "editor/editor_config.hpp"
#include "editor/ui/style.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr int kWindowStyleVars = 2;  // padding and rounding of the hub's only window
constexpr const char* kHubWindowId = "##hub";
constexpr const char* kNewGameId = "new";
constexpr const char* kGamesId = "games";

// The hub's only window: it fills the whole of the system's window, without decorations.
void begin_hub_window() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kHubPadding, kHubPadding));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::Begin(kHubWindowId, nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar(kWindowStyleVars);
}

}  // namespace

void ProjectHub::frame() {
  window_.begin_frame();
  const ImVec4 background = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
  ClearBackground(ColorFromNormalized({background.x, background.y, background.z, background.w}));
  interface_.begin_frame();
  draw();
  interface_.end_frame();
  window_.end_frame();
}

void ProjectHub::draw() {
  begin_hub_window();
  draw_title();
  ImGui::BeginChild(kNewGameId, ImVec2(kHubSidebarWidth, 0.0f));
  draw_new_game(kHubSidebarWidth);
  ImGui::EndChild();
  ImGui::SameLine(0.0f, kHubPadding);
  ImGui::BeginChild(kGamesId);
  draw_games();
  ImGui::EndChild();
  ImGui::End();
}

void ProjectHub::draw_title() {
  ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTitleFontScale);
  ImGui::TextColored(ui::accent_color(), ICON_FA_GEM);
  ImGui::SameLine();
  ImGui::TextUnformatted(kHubTitle);
  ImGui::PopFont();
  ImGui::SameLine();
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
  ImGui::TextColored(ui::muted_color(), "Make 3D games in Ruby");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
}

}  // namespace redgem
