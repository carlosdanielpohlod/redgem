// The Scene view draws a selected NPC's gizmos: from its properties while editing, from the parts
// its scripts gave it while the game runs. The View menu's toggle hides them.

#include <doctest/doctest.h>
#include <raylib.h>

#include <filesystem>
#include <string>

#include "core/config.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/editor_app.hpp"
#include "project/editor_setup.hpp"
#include "window_picture.hpp"

using namespace redgem;
namespace {
constexpr int kFrames = 5;
constexpr int kWidth = 800;
constexpr int kHeight = 600;
constexpr int kCaptureSide = 400;
constexpr std::size_t kPictureBytes = 1024 * 1024;
constexpr int kRedLeast = 200;  // the sight cone's color, and not the selection's orange
constexpr int kOtherMost = 130;
constexpr int kConePixels = 40;
constexpr Vec3 kCameraPosition{0.0f, 14.0f, 0.5f};
constexpr const char* kGuard = "Guard";

void frames(EditorApp& app) {
  for (int frame = 0; frame < kFrames; ++frame) app.frame();
}

std::filesystem::path game_files() {
  const auto game = std::filesystem::temp_directory_path() / "redgem_npc_gizmos_editor";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / "scripts");
  auto scene = SceneDocument::create_empty(game / config::kSceneFileName);
  const int guard = scene.add_empty(kGuard);
  scene.set_prop(guard, "sight", 6.0);
  scene.set_prop(guard, "fov", 90.0);
  scene.set_prop(guard, "hearing", 3.0);
  scene.set_prop(guard, "prompt", std::string("npc.talk"));
  std::string error;
  REQUIRE_MESSAGE(scene.save(error), error);
  return game;
}

// The sight cone's color, and nothing else of the view's.
bool is_red(::Color pixel) { return pixel.r > kRedLeast && pixel.g < kOtherMost && pixel.b < kOtherMost; }

int red_pixels(EditorApp& app) {
  std::string error;
  const auto picture = app.capture_viewport(kCaptureSide, kPictureBytes, error);
  REQUIRE_MESSAGE(picture.has_value(), error);
  const Image image = LoadImageFromMemory(".png", picture->bytes.data(), static_cast<int>(picture->bytes.size()));
  REQUIRE(image.data != nullptr);
  int red = 0;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      if (is_red(GetImageColor(image, x, y))) ++red;
    }
  }
  UnloadImage(image);
  return red;
}

std::unique_ptr<EditorApp> open_editor(const std::filesystem::path& game) {
  auto options = editor_options(GpuChoice::Auto);
  options.clock = [] { return config::kFixedDeltaSeconds; };
  auto created = EditorApp::create(game, options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  auto app = std::move(std::get<std::unique_ptr<EditorApp>>(created));
  SetWindowSize(kWidth, kHeight);
  app->camera().look_at(kCameraPosition, Vec3::zero());
  frames(*app);
  return app;
}
}  // namespace

TEST_CASE("the Scene view draws a selected NPC's sight cone from its properties, and the toggle hides it") {
  const auto game = game_files();
  auto app = open_editor(game);
  CHECK(app->npc_gizmos_shown());
  app->session().select(app->session().scene().find(kGuard));
  app->set_npc_gizmos(false);
  frames(*app);
  CHECK(app->selected_npc_gizmos().empty());
  const int hidden = red_pixels(*app);
  app->set_npc_gizmos(true);
  frames(*app);
  const auto gizmos = app->selected_npc_gizmos();
  REQUIRE(gizmos.size() == 1);
  REQUIRE(gizmos[0].labels.size() == 1);
  CHECK(gizmos[0].labels[0].text == "npc.talk");
  CHECK(red_pixels(*app) > hidden + kConePixels);
  CHECK(test::save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor/npc_gizmos.png"));
  app->session().select(kNoObject);
  frames(*app);
  CHECK(app->selected_npc_gizmos().empty());  // only the selection's are drawn
  CHECK(red_pixels(*app) <= hidden + kConePixels);
  CHECK_FALSE(app->session().document().dirty());
  std::filesystem::remove_all(game);
}

TEST_CASE("while the game runs, a selected NPC shows where its NavAgent was sent") {
  const auto game = game_files();
  auto app = open_editor(game);
  app->play();
  frames(*app);
  const auto result = app->session().runtime()->scripts().eval(
      "Scene.find('Guard').add_component(NavAgent).instance_variable_set(:@destination, Vector3.new(3, 0, 0))",
      "npc_gizmos_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  app->session().select(app->session().scene().find(kGuard));
  frames(*app);
  const auto gizmos = app->selected_npc_gizmos();
  REQUIRE(gizmos.size() == 1);
  CHECK_FALSE(gizmos[0].lines.empty());  // the cross where it goes
  CHECK(gizmos[0].labels.empty());       // its properties alone make no part while the game runs
  app->stop();
  frames(*app);
  std::filesystem::remove_all(game);
}
