#include <doctest/doctest.h>
#include <raylib.h>
#include <filesystem>
#include "core/config.hpp"
#include "core/files.hpp"
#include "editor/editor_app.hpp"
#include "editor/editor_config.hpp"
#include "project/editor_setup.hpp"
#include "scene/props.hpp"
#include "window_picture.hpp"

using namespace redgem;
namespace {
constexpr int kFrames = 5;
// This checks file polling rather than render quality; keep software-rendered CI inexpensive.
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
constexpr int kCaptureSide = 400;
constexpr std::size_t kPictureBytes = 1024 * 1024;
constexpr int kColorMargin = 64;
constexpr int kVisiblePixels = 250;
void write_mover(const std::filesystem::path& game, bool blue) {
  const std::string color = blue ? "0, 0, 1" : "1, 0, 0";
  const std::string code =
      "class Mover < Behaviour\nattr_reader :steps\ndef start; $actor = self; @steps = 0; $starts = ($starts || 0) + "
      "1; end\ndef update(_dt); @steps += 1; game_object.tint = [" +
      color + "]; end\nend\n";
  std::string error;
  REQUIRE_MESSAGE(write_text_file(game / "scripts/mover.rb", code, error), error);
}
std::filesystem::path game_files() {
  const auto game = std::filesystem::temp_directory_path() / "redgem_editor_live_scripts";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / "scripts");
  write_mover(game, false);
  auto doc = SceneDocument::create_empty(game / config::kSceneFileName);
  const int actor = doc.add_primitive(PrimitiveShape::Cube, "Actor", redgem::Color{1, 1, 1, 1});
  doc.set_prop(actor, props::kScript, std::string("mover.rb"));
  redgem::Transform pose;
  pose.position = {0, 2, 0};
  doc.set_local(actor, pose);
  doc.add_light(LightType::Directional, "Sun", redgem::Color{1, 1, 1, 1}, 3);
  const int camera = doc.add_camera("Camera", kPi / 3);
  pose.position.z = 6;
  doc.set_local(camera, pose);
  std::string error;
  REQUIRE_MESSAGE(doc.save(error), error);
  return game;
}
void frames(EditorApp& app, int count = kFrames) {
  for (int i = 0; i < count; ++i) app.frame();
}
int colored_pixels(EditorApp& app, bool blue) {
  std::string error;
  const auto picture = app.capture_viewport(kCaptureSide, kPictureBytes, error);
  REQUIRE_MESSAGE(picture.has_value(), error);
  Image image = LoadImageFromMemory(".png", picture->bytes.data(), static_cast<int>(picture->bytes.size()));
  REQUIRE(image.data != nullptr);
  int pixels = 0;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const auto pixel = GetImageColor(image, x, y);
      const int bright = blue ? pixel.b : pixel.r;
      const int other = blue ? pixel.r : pixel.b;
      if (bright > other + kColorMargin && bright > pixel.g + kColorMargin) ++pixels;
    }
  }
  UnloadImage(image);
  return pixels;
}
void checked(ScriptHost& host, const std::string& code) {
  const auto result = host.eval(code, "reload_window.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
}
}  // namespace
TEST_CASE("saved behaviour changes reach a running editor game without replacing its instances") {
  const auto game = game_files();
  auto options = editor_options(GpuChoice::Auto);
  options.clock = [] { return config::kFixedDeltaSeconds; };
  auto created = EditorApp::create(game, options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  auto& app = *std::get<std::unique_ptr<EditorApp>>(created);
  SetWindowSize(kWindowWidth, kWindowHeight);
  frames(app);
  app.session().select_by_name("Actor");
  app.frame_selection();
  app.play();
  frames(app);
  auto* engine = app.session().runtime();
  REQUIRE(engine != nullptr);
  CHECK(colored_pixels(app, false) > kVisiblePixels);
  checked(engine->scripts(), "$id = $actor.object_id; $steps_before = $actor.steps");
  write_mover(game, true);
  frames(app, 2 * editor_config::kAssetScanFrames + kFrames);
  CHECK(app.session().runtime() == engine);
  checked(engine->scripts(), "raise unless $actor.object_id == $id && $starts == 1 && $actor.steps > $steps_before");
  CHECK(colored_pixels(app, true) > kVisiblePixels);
  CHECK(test::save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor/script_reload_blue.png"));
  CHECK_FALSE(app.session().document().dirty());
  app.stop();
  std::filesystem::remove_all(game);
}
