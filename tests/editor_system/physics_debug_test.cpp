#include <doctest/doctest.h>
#include <raylib.h>

#include <filesystem>

#include "core/config.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/editor_app.hpp"
#include "project/editor_setup.hpp"
#include "scene/props.hpp"
#include "window_picture.hpp"

using namespace redgem;
namespace {
constexpr int kFrames = 5;
constexpr int kWidth = 800;
constexpr int kHeight = 600;
constexpr int kCaptureSide = 400;
constexpr std::size_t kPictureBytes = 1024 * 1024;
constexpr int kGreenMargin = 60;
constexpr int kWirePixels = 40;
void frames(EditorApp& app) {
  for (int frame = 0; frame < kFrames; ++frame) app.frame();
}
std::filesystem::path game_files() {
  const auto game = std::filesystem::temp_directory_path() / "redgem_physics_debug_editor";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / "scripts");
  auto scene = SceneDocument::create_empty(game / config::kSceneFileName);
  const int actor = scene.add_primitive(PrimitiveShape::Sphere, "Actor", redgem::Color{1, 0, 0, 1});
  scene.set_prop(actor, props::kBody, std::string(props::kBodyDynamic));
  scene.set_prop(actor, props::kCollider, std::string("box"));
  redgem::Transform pose;
  pose.position = {0, 2, 0};
  scene.set_local(actor, pose);
  scene.add_light(LightType::Directional, "Sun", redgem::Color{1, 1, 1, 1}, 3);
  const int camera = scene.add_camera("Camera", kPi / 3);
  pose.position.z = 6;
  scene.set_local(camera, pose);
  std::string error;
  REQUIRE_MESSAGE(scene.save(error), error);
  return game;
}
int green_pixels(EditorApp& app) {
  std::string error;
  const auto picture = app.capture_viewport(kCaptureSide, kPictureBytes, error);
  REQUIRE_MESSAGE(picture.has_value(), error);
  const Image image = LoadImageFromMemory(".png", picture->bytes.data(), static_cast<int>(picture->bytes.size()));
  REQUIRE(image.data != nullptr);
  int green = 0;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const auto pixel = GetImageColor(image, x, y);
      if (pixel.g > pixel.r + kGreenMargin && pixel.g > pixel.b + kGreenMargin) ++green;
    }
  }
  UnloadImage(image);
  return green;
}
}  // namespace
TEST_CASE("the editor can show actual collider wires while paused and remove them without editing the scene") {
  const auto game = game_files();
  auto options = editor_options(GpuChoice::Auto);
  options.clock = [] { return config::kFixedDeltaSeconds; };
  auto created = EditorApp::create(game, options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  auto& app = *std::get<std::unique_ptr<EditorApp>>(created);
  SetWindowSize(kWidth, kHeight);
  frames(app);
  CHECK_FALSE(app.physics_debug_enabled());
  app.play();
  frames(app);
  app.toggle_pause();
  frames(app);
  REQUIRE(app.session().play_state() == PlayState::Paused);
  const int baseline = green_pixels(app);
  app.set_physics_debug(true);
  frames(app);
  CHECK(green_pixels(app) > baseline + kWirePixels);
  CHECK(test::save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor/physics_colliders.png"));
  app.set_physics_debug(false);
  frames(app);
  CHECK(green_pixels(app) <= baseline + kWirePixels);
  CHECK_FALSE(app.session().document().dirty());
  app.stop();
  frames(app);
  std::filesystem::remove_all(game);
}
