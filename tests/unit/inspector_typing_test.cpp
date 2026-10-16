// A value typed into an Inspector field (Ctrl+click on a number) belongs to the object it was
// typed for, whatever gets selected next: driven through ImGui itself, with no window, the way the
// editor draws its panels (the Hierarchy before the Inspector, the Scene view after it).

#include <doctest/doctest.h>

#include <imgui.h>

#include <filesystem>
#include <functional>

#include "editor/document/scene_document.hpp"
#include "editor/edit_stream.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/inspector_cards.hpp"

using namespace redgem;

namespace {

constexpr float kFrameSeconds = 1.0f / 60.0f;
const ImVec2 kScreen{1280.0f, 720.0f};
const ImVec2 kPanelSize{300.0f, 400.0f};
const ImVec2 kHierarchyAt{0.0f, 0.0f};
const ImVec2 kInspectorAt{320.0f, 0.0f};
const ImVec2 kSceneAt{640.0f, 0.0f};
constexpr float kInsideField = 8.0f;  // pixels in from a field's left edge: its first number, x
constexpr char kTyped = '7';
constexpr float kTypedValue = 7.0f;
constexpr int kSettleFrames = 5;
constexpr int kMostClicks = 3;  // a click away from a field being typed in may only end the typing
constexpr int kRowFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                          ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                          ImGuiTreeNodeFlags_SpanFullWidth;  // as the Hierarchy draws its rows

ImVec2 middle(const ImVec2& min, const ImVec2& max) { return {(min.x + max.x) / 2, (min.y + max.y) / 2}; }

// An interface nobody sees, and two objects to edit in it.
struct Editor {
  SceneDocument doc = SceneDocument::create_empty(std::filesystem::temp_directory_path() / "redgem_typing.glb");
  int first = doc.add_empty("First");
  int second = doc.add_empty("Second");
  int selection = first;
  EditStream stream;
  ImVec2 row_of_second;
  ImVec2 field;  // the selection's Position, x
  ImVec2 scene_view;

  Editor() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = kScreen;
    io.DeltaTime = kFrameSeconds;
    io.ConfigMacOSXBehaviors = false;  // Ctrl is Ctrl on every machine the tests run on
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    frame();
  }
  ~Editor() { ImGui::DestroyContext(); }

  float x_of(int object) { return doc.node(object).local.position.x; }

  void frame() {
    ImGui::NewFrame();
    hierarchy();
    inspector();
    stream.end_frame(ImGui::IsAnyItemActive());
    scene();
    ImGui::Render();
  }

  void frames(int count) {
    for (int i = 0; i < count; ++i) frame();
  }

  static void panel(const char* name, const ImVec2& at) {
    ImGui::SetNextWindowPos(at);
    ImGui::SetNextWindowSize(kPanelSize);
    ImGui::Begin(name);
  }

  void hierarchy() {
    panel(editor_config::kHierarchyWindow, kHierarchyAt);
    for (const int object : {first, second}) {
      ImGui::PushID(object);
      ImGui::TreeNodeEx("##node", kRowFlags | (object == selection ? ImGuiTreeNodeFlags_Selected : 0));
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) selection = object;
      if (object == second) row_of_second = middle(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
      ImGui::PopID();
    }
    ImGui::End();
  }

  // The Transform card's Position row, as the Inspector edits it.
  void inspector() {
    panel(editor_config::kInspectorWindow, kInspectorAt);
    Transform local = doc.node(selection).local;
    const bool changed = ui::inspector::drag_vec3("Position", "##position", local.position, 1.0f);
    field = {ImGui::GetItemRectMin().x + kInsideField, middle(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()).y};
    if (changed) {
      stream.before_change(doc, selection);
      doc.set_local(selection, local, false);
    }
    ImGui::End();
  }

  // Picking: a click in the Scene view selects what is under it (here, always the second object).
  void scene() {
    panel(editor_config::kViewportWindow, kSceneAt);
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) selection = second;
    scene_view = middle(ImGui::GetWindowPos(), {ImGui::GetWindowPos().x + kPanelSize.x, kPanelSize.y});
    ImGui::End();
  }

  void move_to(const ImVec2& at) {
    ImGui::GetIO().AddMousePosEvent(at.x, at.y);
    frame();
  }

  void click() {
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame();
  }

  // Ctrl+click on the selection's x and a digit typed, with no Enter.
  void type_into_field() {
    move_to(field);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
    frame();
    click();
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
    ImGui::GetIO().AddInputCharacter(kTyped);
    frames(kSettleFrames);
  }

  // Clicks somewhere until the second object is selected.
  void select_second_at(const ImVec2& at) {
    move_to(at);
    for (int i = 0; i < kMostClicks && selection != second; ++i) click();
    frames(kSettleFrames);
  }
};

void typed_then_selected_elsewhere(const std::function<ImVec2(const Editor&)>& where) {
  Editor editor;
  editor.type_into_field();
  REQUIRE(editor.x_of(editor.first) == doctest::Approx(kTypedValue));  // typing shows as it goes
  editor.select_second_at(where(editor));
  REQUIRE(editor.selection == editor.second);
  CHECK(editor.x_of(editor.second) == doctest::Approx(0.0f));
  CHECK(editor.x_of(editor.first) == doctest::Approx(kTypedValue));
  // The typing was one undo step, of the object it was typed for.
  REQUIRE(editor.doc.undo());
  CHECK(editor.x_of(editor.first) == doctest::Approx(0.0f));
  CHECK(editor.x_of(editor.second) == doctest::Approx(0.0f));
}

}  // namespace

TEST_CASE("a value typed into the Inspector stays with its object when another one is clicked in the Hierarchy") {
  typed_then_selected_elsewhere([](const Editor& editor) { return editor.row_of_second; });
}

TEST_CASE("a value typed into the Inspector stays with its object when another one is picked in the Scene view") {
  typed_then_selected_elsewhere([](const Editor& editor) { return editor.scene_view; });
}
