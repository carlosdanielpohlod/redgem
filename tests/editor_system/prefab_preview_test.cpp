#include <doctest/doctest.h>
#include <raylib.h>

#include <filesystem>
#include <algorithm>
#include <string>

#include "core/config.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/editor_app.hpp"
#include "project/editor_setup.hpp"
#include "scene/props.hpp"
#include "window_picture.hpp"

using namespace redgem;
namespace {
constexpr int kFrames = 5;  // let layout and swapped buffers settle
constexpr int kCaptureSide = 400;
constexpr std::size_t kCaptureBytes = 1024 * 1024;
constexpr int kColorMargin = 64;
constexpr int kVisiblePixels = 250;  // substantially more than a gizmo axis
constexpr float kSphereFill = 0.9f;  // a front-facing sphere leaves the corners of its bounding box empty
constexpr Vec3 kPlacement{0, 2, 0};
constexpr Vec3 kSourceOffset{2, 0, 0};
constexpr redgem::Color kRed{1, 0, 0, 1};
constexpr redgem::Color kBlue{0, 0, 1, 1};

void source_file(const std::filesystem::path& game, PrimitiveShape shape, redgem::Color color) {
  auto source = SceneDocument::create_empty(game / "prefabs/shape.glb");
  const int root = source.add_empty("Source");
  source.set_prop(root, props::kPrefab, true);
  const int child = source.add_primitive(shape, "Inherited", color, root);
  redgem::Transform pose;
  pose.position = kSourceOffset;
  source.set_local(child, pose);
  std::string error;
  REQUIRE_MESSAGE(source.save(error), error);
}
std::filesystem::path game_files() {
  const auto game = std::filesystem::temp_directory_path() / "redgem_prefab_editor_window";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / "prefabs");
  source_file(game, PrimitiveShape::Cube, kRed);
  auto scene = SceneDocument::create_empty(game / config::kSceneFileName);
  scene.add_light(LightType::Directional, "Sun", redgem::Color{1, 1, 1, 1}, 3);
  const int camera = scene.add_camera("Camera", kPi / 3);
  redgem::Transform pose;
  pose.position = kPlacement + kSourceOffset + Vec3{0, 0, 6};
  scene.set_local(camera, pose);
  const int placed = scene.add_empty("Placed");
  pose.position = kPlacement;
  scene.set_local(placed, pose);
  scene.set_prop(placed, props::kPrefabFile, std::string("prefabs/shape.glb"));
  std::string error;
  REQUIRE_MESSAGE(scene.save(error), error);
  return game;
}
void frames(EditorApp& app) {
  for (int i = 0; i < kFrames; ++i) app.frame();
}
bool colored(::Color pixel, bool blue) {
  const int bright = blue ? pixel.b : pixel.r;
  const int other = blue ? pixel.r : pixel.b;
  return bright > other + kColorMargin && bright > pixel.g + kColorMargin;
}
struct Silhouette {
  int pixels = 0;
  int min_x = kCaptureSide;
  int min_y = kCaptureSide;
  int max_x = 0;
  int max_y = 0;
  float fill() const {
    return static_cast<float>(pixels) / static_cast<float>((max_x - min_x + 1) * (max_y - min_y + 1));
  }
};
Silhouette silhouette(EditorApp& app, bool blue) {
  std::string error;
  const auto picture = app.capture_viewport(kCaptureSide, kCaptureBytes, error);
  REQUIRE_MESSAGE(picture.has_value(), error);
  Image image = LoadImageFromMemory(".png", picture->bytes.data(), static_cast<int>(picture->bytes.size()));
  REQUIRE(image.data != nullptr);
  Silhouette result;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      if (!colored(GetImageColor(image, x, y), blue)) continue;
      ++result.pixels;
      result.min_x = std::min(result.min_x, x);
      result.max_x = std::max(result.max_x, x);
      result.min_y = std::min(result.min_y, y);
      result.max_y = std::max(result.max_y, y);
    }
  }
  UnloadImage(image);
  return result;
}
void picture(EditorApp& app, const std::string& name) {
  frames(app);
  CHECK(test::save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor" / (name + ".png")));
}
}  // namespace
TEST_CASE("prefabs render in the editor through selection play stop and same-count source replacements") {
  const auto game = game_files();
  EditorOptions options = editor_options(GpuChoice::Auto);
  options.clock = [] { return config::kFixedDeltaSeconds; };
  auto created = EditorApp::create(game, options);
  if (const auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  auto& app = *std::get<std::unique_ptr<EditorApp>>(created);
  auto& session = app.session();
  frames(app);
  const int placed = session.scene_desc().find_node("Placed");
  session.select(placed);
  app.frame_selection();
  CHECK(app.camera().target().x == doctest::Approx(kSourceOffset.x));
  session.select(kNoObject);
  frames(app);
  CHECK(silhouette(app, false).pixels > kVisiblePixels);
  CHECK(app.pick_at(app.viewport_width() * kHalf, app.viewport_height() * kHalf) == placed);
  CHECK(session.selection() == placed);
  session.select(kNoObject);
  picture(app, "prefab_preview_cube");
  app.play();
  frames(app);
  CHECK(silhouette(app, false).pixels > kVisiblePixels);
  app.stop();
  frames(app);
  CHECK(silhouette(app, false).pixels > kVisiblePixels);
  source_file(game, PrimitiveShape::Sphere, kBlue);
  session.refresh_preview();
  frames(app);
  const auto sphere = silhouette(app, true);
  REQUIRE(sphere.pixels > kVisiblePixels);
  CHECK(sphere.fill() < kSphereFill);
  picture(app, "prefab_preview_sphere");
  std::string error;
  REQUIRE_MESSAGE(session.save(error), error);
  auto saved = SceneDocument::open(game / config::kSceneFileName);
  REQUIRE(std::holds_alternative<SceneDocument>(saved));
  CHECK(std::get<SceneDocument>(saved).desc().meshes.empty());
  std::filesystem::remove_all(game);
}
