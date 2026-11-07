#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>

#include "core/config.hpp"
#include "editor/editor_app.hpp"
#include "editor/scene_recovery.hpp"
#include "project/editor_setup.hpp"
#include "window_picture.hpp"

using namespace redgem;
namespace {
constexpr int kFrames = 5;  // settle docking and GPU presentation, without waiting out the autosave interval
void settle(EditorApp& app) {
  for (int i = 0; i < kFrames; ++i) app.frame();
}
std::unique_ptr<EditorApp> editor(const std::filesystem::path& game) {
  auto result = EditorApp::create(game, editor_options(GpuChoice::Auto));
  if (auto* error = std::get_if<std::string>(&result)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<EditorApp>>(result));
}
}  // namespace
TEST_CASE("the editor offers crash recovery and saves the recovered scene to its original file") {
  const auto game = std::filesystem::temp_directory_path() / "redgem_editor_recovery_window";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game);
  const auto file = game / config::kSceneFileName;
  auto document = SceneDocument::create_empty(file);
  document.add_primitive(PrimitiveShape::Cube, "Original", redgem::Color{0, 0, 1, 1});
  std::string error;
  REQUIRE(document.save(error));
  auto app = editor(game);
  settle(*app);
  app->session().document().set_name(0, "Recovered cube");
  app->session().autosave(SceneRecovery::kIntervalSeconds);
  REQUIRE(std::filesystem::exists(SceneRecovery::copy_path(file)));
  app.reset();  // abrupt shutdown: no Save or Discard command
  app = editor(game);
  settle(*app);
  REQUIRE(app->session().recovery_pending());
  const auto* popup = ImGui::FindWindowByName("Recover scene###scene_recovery");
  REQUIRE(popup != nullptr);
  CHECK(popup->Active);
  CHECK(test::save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor/recovery_offered.png"));
  REQUIRE(app->recover_scene());
  settle(*app);
  CHECK(app->session().document().node(0).name == "Recovered cube");
  CHECK(app->session().document().dirty());
  CHECK(app->session().document().path() == file);
  app->session().document().set_name(0, "Saved cube");
  app->save();
  CHECK_FALSE(app->session().document().dirty());
  CHECK_FALSE(std::filesystem::exists(SceneRecovery::copy_path(file)));
  settle(*app);
  CHECK(test::save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor/recovery_saved.png"));
  app.reset();
  auto opened = SceneDocument::open(file);
  REQUIRE(std::holds_alternative<SceneDocument>(opened));
  CHECK(std::get<SceneDocument>(opened).node(0).name == "Saved cube");
  std::filesystem::remove_all(game);
}
