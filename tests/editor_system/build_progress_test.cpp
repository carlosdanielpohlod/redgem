// System test: a build in the real editor window runs in the background while the editor keeps
// drawing, shows how it goes and how it ended, and holds the editor open until it is done.
#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/config.hpp"
#include "editor/editor_app.hpp"
#include "window_picture.hpp"

using namespace redgem;
using redgem::test::save_window_picture;

namespace {

constexpr int kWarmupFrames = 10;
constexpr int kBusyFrames = 30;                       // frames drawn while the build is held
constexpr int kPictureFrames = 3;                     // more than the buffers a window swaps between
constexpr int kMostFrames = 600;                      // frames a build that was let go may take to be taken in
constexpr auto kPatience = std::chrono::seconds(10);  // far longer than the fake build takes to begin
constexpr const char* kTarget = "android";
constexpr const char* kStep = "packing the game's files";
constexpr const char* kStatusWindow = "##build_status";
constexpr const char* kWaitDialog = "###build_wait";

std::filesystem::path copy_game() {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_editor_build" / "game";
  std::filesystem::remove_all(dir.parent_path());
  std::filesystem::create_directories(dir.parent_path());
  std::filesystem::copy(REDGEM_PLATFORMER_DIR, dir, std::filesystem::copy_options::recursive);
  return dir;
}

// A build that says a step, then waits to be let go: as long as a build for a phone takes. (What
// it waits on is shared with its thread, which may outlast the test's own variables.)
struct HeldBuild {
  struct Signals {
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    std::promise<void> began;
  };
  std::shared_ptr<Signals> signals = std::make_shared<Signals>();
  std::future<void> has_begun = signals->began.get_future();

  void let_go() { signals->release.set_value(); }

  BuildChoice choice() const {
    BuildChoice choice;
    choice.name = kTarget;
    choice.label = kTarget;
    choice.build = [signals = signals](const std::filesystem::path&, const std::filesystem::path& out_dir,
                                       const BuildProgress& progress,
                                       std::string&) -> std::optional<std::filesystem::path> {
      progress(kStep);
      signals->began.set_value();
      signals->released.wait_for(kPatience);  // (a test that fails before letting it go does not hang)
      return out_dir / kTarget;
    };
    return choice;
  }
};

// Every frame counts as one fixed step, however long the machine took to draw it.
EditorOptions even_pace() {
  EditorOptions options;
  options.clock = [] { return config::kFixedDeltaSeconds; };
  return options;
}

void run(EditorApp& app, int frames) {
  for (int i = 0; i < frames; ++i) app.frame();
}

// Draws until the build was taken in, or too many frames went by.
void run_until_built(EditorApp& app) {
  for (int i = 0; i < kMostFrames && app.building(); ++i) app.frame();
}

// Whether a window of the interface was drawn in the last frame.
bool shown(const char* window) {
  const ImGuiWindow* found = ImGui::FindWindowByName(window);
  return found != nullptr && found->WasActive;
}

}  // namespace

TEST_CASE("a build runs in the background: the editor keeps drawing, shows its step, then how it ended") {
  const auto game = copy_game();
  auto created = EditorApp::create(game, even_pace());
  REQUIRE(std::holds_alternative<std::unique_ptr<EditorApp>>(created));
  auto& app = *std::get<std::unique_ptr<EditorApp>>(created);
  run(app, kWarmupFrames);

  HeldBuild held;
  REQUIRE(app.start_build(held.choice()));
  REQUIRE(held.has_begun.wait_for(kPatience) == std::future_status::ready);
  const int frames_before = ImGui::GetFrameCount();
  run(app, kBusyFrames);
  CHECK(ImGui::GetFrameCount() == frames_before + kBusyFrames);  // drawn while the build waits
  CHECK(app.building());
  CHECK(shown(kStatusWindow));
  for (int i = 0; i < kPictureFrames; ++i) app.frame();
  CHECK(save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor" / "editor_building.png"));

  const auto errors = app.console().error_count();
  const HeldBuild another;
  CHECK_FALSE(app.start_build(another.choice()));  // one build at a time
  CHECK(app.console().error_count() == errors + 1);

  held.let_go();
  run_until_built(app);
  REQUIRE_FALSE(app.building());
  REQUIRE(app.last_build().has_value());
  CHECK(app.last_build()->succeeded());
  CHECK(app.last_build()->out_dir == game.parent_path() / ("game-build-" + std::string(kTarget)));
  run(app, kPictureFrames);
  CHECK(shown(kStatusWindow));  // now with how it ended
}

TEST_CASE("leaving while a build runs waits for it, saying so, and leaves once it is done") {
  const auto game = copy_game();
  auto created = EditorApp::create(game, even_pace());
  REQUIRE(std::holds_alternative<std::unique_ptr<EditorApp>>(created));
  auto& app = *std::get<std::unique_ptr<EditorApp>>(created);
  run(app, kWarmupFrames);

  HeldBuild held;
  REQUIRE(app.start_build(held.choice()));
  app.request_exit(EditorExit::Quit);
  run(app, kBusyFrames);
  CHECK_FALSE(app.finished());
  CHECK(shown(kWaitDialog));

  held.let_go();
  for (int i = 0; i < kMostFrames && !app.finished(); ++i) app.frame();
  CHECK(app.finished());
  CHECK(app.last_build().has_value());
}
