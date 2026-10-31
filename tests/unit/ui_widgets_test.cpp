// The editor's own widgets, driven through ImGui itself with no window on screen.

#include <doctest/doctest.h>

#include <imgui.h>
#include <imgui_internal.h>

#include "editor/editor_config.hpp"
#include "editor/ui/style.hpp"

using namespace redgem;

namespace {

const ImVec2 kScreen{1280.0f, 720.0f};
constexpr float kFrameSeconds = 1.0f / 60.0f;
constexpr const char* kToolbar = "Toolbar";
constexpr const char* kInspector = "Inspector";
constexpr const char* kFirstTip = "Hierarchy";
constexpr const char* kSecondTip = "Inspector";
constexpr const char* kField = "field";

// An interface nobody sees, inside one frame.
struct Interface {
  Interface() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = kScreen;
    io.DeltaTime = kFrameSeconds;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();
  }
  ~Interface() {
    ImGui::Render();
    ImGui::DestroyContext();
  }
};

ImGuiID button_id(const char* icon, const char* tip) {
  ui::icon_button(icon, tip);
  return ImGui::GetItemID();
}

// Makes an item of the named window the one being used, as a click into a text box would.
void use_item_in(const char* window_name) {
  ImGui::Begin(window_name);
  ImGui::SetActiveID(ImGui::GetID(kField), ImGui::GetCurrentWindow());
  ImGui::End();
}

}  // namespace

TEST_CASE("two icon buttons with the same icon get different IDs") {
  Interface interface;
  ImGui::Begin(kToolbar);
  const ImGuiID first = button_id(ICON_FA_TABLE_LIST, kFirstTip);
  const ImGuiID second = button_id(ICON_FA_TABLE_LIST, kSecondTip);
  ImGui::End();
  CHECK(first != second);
}

TEST_CASE("typing in the Agent panel is not using the rest of the editor") {
  Interface interface;
  CHECK_FALSE(ui::item_active_outside(editor_config::kAgentWindow));
  use_item_in(editor_config::kAgentWindow);
  CHECK_FALSE(ui::item_active_outside(editor_config::kAgentWindow));
  use_item_in(kInspector);
  CHECK(ui::item_active_outside(editor_config::kAgentWindow));
}
