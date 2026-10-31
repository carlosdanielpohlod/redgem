// Icon buttons drawn with the same icon in one window (the Hierarchy and Inspector toggles) are
// told apart by what they do, so ImGui never sees two items with one ID.

#include <doctest/doctest.h>

#include <imgui.h>

#include "editor/ui/style.hpp"

using namespace redgem;

namespace {

const ImVec2 kScreen{1280.0f, 720.0f};
constexpr float kFrameSeconds = 1.0f / 60.0f;
constexpr const char* kWindow = "Toolbar";
constexpr const char* kFirstTip = "Hierarchy";
constexpr const char* kSecondTip = "Inspector";

// A window nobody sees, as the editor's toolbar draws one.
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
    ImGui::Begin(kWindow);
  }
  ~Interface() {
    ImGui::End();
    ImGui::Render();
    ImGui::DestroyContext();
  }
};

ImGuiID button_id(const char* icon, const char* tip) {
  ui::icon_button(icon, tip);
  return ImGui::GetItemID();
}

}  // namespace

TEST_CASE("two icon buttons with the same icon get different IDs") {
  Interface interface;
  const ImGuiID first = button_id(ICON_FA_TABLE_LIST, kFirstTip);
  const ImGuiID second = button_id(ICON_FA_TABLE_LIST, kSecondTip);
  CHECK(first != second);
}
