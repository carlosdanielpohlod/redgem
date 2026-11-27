// Pose preview in the editor's Scene view: a skinned model posed by its clip at a chosen time,
// and back at rest once deselected; the saved scene keeps the rest pose.
#include <doctest/doctest.h>
#include <raylib.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "core/config.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/editor_app.hpp"
#include "project/editor_setup.hpp"
#include "temp_dirs.hpp"
#include "window_picture.hpp"

using namespace redgem;
namespace {

constexpr int kFrames = 5;  // let layout and swapped buffers settle
constexpr int kCaptureSide = 400;
constexpr std::size_t kCaptureBytes = 1024 * 1024;
constexpr int kChannelMargin = 24;     // a pixel differs when a channel moves by more than this
constexpr int kMovedPixels = 200;      // the arm's pose moves at least this many pixels
constexpr int kSamePixelsAtMost = 20;  // back at rest: the picture is (nearly) the one before
constexpr float kLastKey = 1.0f;       // seconds into the clip
constexpr float kSunIntensity = 3.0f;

std::filesystem::path game_files() {
  const auto game = redgem::test::temp_root() / "redgem_pose_preview_editor_window";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game);
  auto scene = SceneDocument::create_empty(game / config::kSceneFileName);
  scene.add_light(LightType::Directional, "Sun", redgem::Color{1, 1, 1, 1}, kSunIntensity);
  std::string error;
  REQUIRE_MESSAGE(scene.import_model(std::filesystem::path(REDGEM_FIXTURES_DIR) / "skinned.glb", error) != kNoIndex,
                  error);
  REQUIRE_MESSAGE(scene.save(error), error);
  return game;
}

void frames(EditorApp& app) {
  for (int i = 0; i < kFrames; ++i) app.frame();
}

Image viewport(EditorApp& app, const std::string& name) {
  frames(app);
  CHECK(test::save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor" / (name + ".png")));
  std::string error;
  const auto picture = app.capture_viewport(kCaptureSide, kCaptureBytes, error);
  REQUIRE_MESSAGE(picture.has_value(), error);
  Image image = LoadImageFromMemory(".png", picture->bytes.data(), static_cast<int>(picture->bytes.size()));
  REQUIRE(image.data != nullptr);
  return image;
}

bool differs(::Color a, ::Color b) {
  return std::abs(a.r - b.r) > kChannelMargin || std::abs(a.g - b.g) > kChannelMargin ||
         std::abs(a.b - b.b) > kChannelMargin;
}

int differing_pixels(const Image& a, const Image& b) {
  REQUIRE(a.width == b.width);
  REQUIRE(a.height == b.height);
  int count = 0;
  for (int y = 0; y < a.height; ++y) {
    for (int x = 0; x < a.width; ++x) count += differs(GetImageColor(a, x, y), GetImageColor(b, x, y)) ? 1 : 0;
  }
  return count;
}

}  // namespace

TEST_CASE("a skinned model is posed in the Scene view at a chosen time, and comes back to rest") {
  const auto game = game_files();
  EditorOptions options = editor_options(GpuChoice::Auto);
  options.clock = [] { return config::kDefaultFixedDeltaSeconds; };
  auto created = EditorApp::create(game, options);
  if (const auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  auto& app = *std::get<std::unique_ptr<EditorApp>>(created);
  auto& session = app.session();
  const ObjectId rig = session.scene().find("Rig");
  REQUIRE(rig != kNoObject);
  session.select(rig);
  app.frame_selection();
  Image rest = viewport(app, "pose_preview_rest");
  const std::vector<int> clips = session.clips_moving(rig);
  REQUIRE(clips.size() == 1);
  REQUIRE(session.preview_pose(rig, clips[0], kLastKey));
  Image posed = viewport(app, "pose_preview_posed");
  CHECK(differing_pixels(rest, posed) > kMovedPixels);
  session.select(kNoObject);
  frames(app);
  session.select(rig);
  Image again = viewport(app, "pose_preview_rest_again");
  CHECK(differing_pixels(rest, again) <= kSamePixelsAtMost);
  UnloadImage(rest);
  UnloadImage(posed);
  UnloadImage(again);
  std::string error;
  REQUIRE_MESSAGE(session.save(error), error);
  std::filesystem::remove_all(game);
}
