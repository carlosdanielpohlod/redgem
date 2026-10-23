#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::editor_logic;
using redgem::test::kTolerance;

namespace {

constexpr float kBarWidth = 400.0f;  // pixels
constexpr float kGroupWidth = 100.0f;
constexpr float kGroupStart = 150.0f;  // where a group that wide starts when centered in the bar
constexpr float kViewX = 300.0f;       // a Scene view somewhere in the window, pixels
constexpr float kViewY = 80.0f;
constexpr float kViewHeight = 600.0f;
constexpr float kTextWidth = 220.0f;
constexpr float kTextHeight = 15.0f;
constexpr float kBothSides = 2.0f;       // the pill pads its text on the two sides
constexpr float kTile = 100.0f;          // pixels: a tile's width
constexpr float kGap = 10.0f;            // between tiles
constexpr float kRoomForThree = 320.0f;  // three tiles and their two gaps
constexpr float kRoomForTwoAndABit = 319.0f;
constexpr float kTooNarrow = 50.0f;
constexpr int kThreeColumns = 3;
constexpr int kTwoColumns = 2;
constexpr int kUnknown = 7;  // what index_of answers for a name it does not know
constexpr int kWood = 4;     // material indices
constexpr int kMetal = 2;
constexpr Vec3 kBoxMin{-1.0f, -2.0f, -3.0f};
constexpr Vec3 kBoxMax{1.0f, 2.0f, 3.0f};
constexpr Vec3 kPlace{10.0f, 0.0f, -5.0f};
constexpr Vec3 kTwiceAsBig{2.0f, 2.0f, 2.0f};
constexpr int kAllMax = 7;  // the corner with every bit set
constexpr int kMaxOnY = 2;  // the corner with only the y bit set
constexpr float kQuarterTurnDegrees = 90.0f;
constexpr Vec3 kCameraPlace{0.0f, 10.0f, 0.0f};
constexpr Vec3 kDownAndForward{0.0f, -0.5f, -0.5f};
constexpr Vec3 kHitOnGround{0.0f, 0.0f, -10.0f};
constexpr Vec3 kAlmostLevel{0.0f, -0.001f, -1.0f};  // meets the ground ten kilometers away
constexpr Vec3 kUpwards{0.0f, 0.5f, -0.5f};
constexpr Vec3 kSpawn{1.0f, 2.0f, 3.0f};
constexpr float kStrength = 4.0f;  // meters per second
constexpr float kHeldSeconds = 0.5f;
constexpr float kRaisedMeters = 2.0f;  // strength x seconds
constexpr float kBrushRadius = 6.0f;   // meters, in the world
constexpr Vec3 kTerrainPlace{100.0f, 0.0f, 50.0f};
constexpr Vec3 kTerrainScale{2.0f, 4.0f, 2.0f};
constexpr Vec3 kPointOnTerrain{104.0f, 0.0f, 56.0f};
constexpr float kLocalX = 2.0f;  // that point in the terrain's own space
constexpr float kLocalZ = 3.0f;
constexpr float kLocalRadius = 3.0f;               // the radius, halved by the terrain's scale
constexpr float kLocalMeters = 0.5f;               // two meters, quartered by its height scale
constexpr Vec3 kMirroredScale{-2.0f, 0.0f, 3.0f};  // one part below the smallest scale, one at nothing
constexpr float kSmallestScale = 0.001f;
constexpr Vec3 kUsableScale{0.001f, 0.001f, 3.0f};
constexpr const char* kFirstScript = "player.rb";
constexpr const char* kMiddleScript = "enemy_ai.rb";
constexpr const char* kLastScript = "coin.rb";
constexpr std::size_t kMiddle = 1;
constexpr const char* kBodies[] = {"static", "dynamic", "vehicle"};
constexpr int kBodyCount = 3;

Transform turned_and_scaled() {
  Transform transform;
  transform.position = kPlace;
  transform.rotation = quat_from_axis_angle(Vec3::up(), degrees_to_radians(kQuarterTurnDegrees));
  transform.scale = kTwiceAsBig;
  return transform;
}

PrimitiveDesc part_with(int material) {
  PrimitiveDesc part;
  part.material = material;
  return part;
}

}  // namespace

TEST_CASE("the Scene view's hint follows what the mouse would do there") {
  using editor_logic::ViewportHint;
  using editor_logic::ViewportState;
  CHECK(editor_logic::viewport_hint(ViewportState{}) == ViewportHint::Fly);
  CHECK(editor_logic::viewport_hint({false, false, true, false}) == ViewportHint::TerrainSelected);
  CHECK(editor_logic::viewport_hint({false, false, true, true}) == ViewportHint::Sculpting);
  // The brush is on but something else is selected: the mouse picks and moves as usual.
  CHECK(editor_logic::viewport_hint({false, false, false, true}) == ViewportHint::Fly);
  CHECK(editor_logic::viewport_hint({true, true, true, true}) == ViewportHint::GameHasKeyboard);
  CHECK(editor_logic::viewport_hint({true, false, false, false}) == ViewportHint::ClickTheGame);
}

TEST_CASE("the window title names the game and its open scene, and marks unsaved changes and play mode") {
  const std::string plain = window_title("demo", "main", false, false);
  CHECK(plain == std::string(editor_config::kWindowTitle) + " - demo - main");
  CHECK(window_title("demo", "level2", false, false) == std::string(editor_config::kWindowTitle) + " - demo - level2");
  CHECK(window_title("demo", "main", true, false) == plain + " *");
  CHECK(window_title("demo", "main", false, true) == plain + " [playing]");
  CHECK(window_title("demo", "main", true, true) == plain + " * [playing]");
}

TEST_CASE("a build goes next to the game, in a folder named after the system when it is another one") {
  CHECK(build_folder_name("demo", "linux", true) == "demo-build");
  CHECK(build_folder_name("demo", "windows", false) == "demo-build-windows");
}

TEST_CASE("paths under the home folder are shown and typed with a tilde") {
  CHECK(pretty_path("/home/ana/games/demo", "/home/ana") == "~/games/demo");
  CHECK(pretty_path("/srv/games/demo", "/home/ana") == "/srv/games/demo");
  CHECK(pretty_path("/srv/games/demo", "") == "/srv/games/demo");
  CHECK(expand_home("~/games/demo", "/home/ana") == std::filesystem::path("/home/ana/games/demo"));
  CHECK(expand_home("/srv/games/demo", "/home/ana") == std::filesystem::path("/srv/games/demo"));
  CHECK(expand_home("games/~/demo", "/home/ana") == std::filesystem::path("games/~/demo"));
}

TEST_CASE("names are capitalized for titles and compared without case when searching") {
  CHECK(capitalized("platformer") == "Platformer");
  CHECK(capitalized("Empty") == "Empty");
  CHECK(lowercase("Main Camera") == "main camera");
  CHECK(name_contains("Main Camera", "cam"));
  CHECK(name_contains("Main Camera", ""));
  CHECK_FALSE(name_contains("Main Camera", "light"));
}

TEST_CASE("something centered starts half of the free room in") {
  CHECK(centered_start(kBarWidth, kGroupWidth) == doctest::Approx(kGroupStart));
  CHECK(centered_start(kGroupWidth, kGroupWidth) == doctest::Approx(0.0f));
}

TEST_CASE("the viewport hint sits in the bottom-left corner, padded around its text") {
  const Rect pill = hint_pill(kViewX, kViewY, kViewHeight, kTextWidth, kTextHeight);
  CHECK(pill.min_x == doctest::Approx(kViewX + editor_config::kOverlayMargin));
  CHECK(pill.max_y == doctest::Approx(kViewY + kViewHeight - editor_config::kOverlayMargin));
  CHECK(pill.max_x - pill.min_x == doctest::Approx(kTextWidth + editor_config::kOverlayPadding * kBothSides));
  CHECK(pill.max_y - pill.min_y == doctest::Approx(kTextHeight + editor_config::kOverlayPadding * kBothSides));
}

TEST_CASE("as many tiles as fit make a row, and there is always one") {
  CHECK(tile_columns(kRoomForThree, kGap, kTile) == kThreeColumns);
  CHECK(tile_columns(kRoomForTwoAndABit, kGap, kTile) == kTwoColumns);
  CHECK(tile_columns(kTooNarrow, kGap, kTile) == 1);
}

TEST_CASE("the Inspector's split leaves it its share of the whole window") {
  const float left_of_hierarchy = 1.0f - editor_config::kHierarchyWidth;
  CHECK(inspector_split_ratio() * left_of_hierarchy == doctest::Approx(editor_config::kInspectorWidth));
}

TEST_CASE("a name is found in a list, with a fallback for names that are not in it") {
  CHECK(index_of(kBodies, kBodyCount, "static", kUnknown) == 0);
  CHECK(index_of(kBodies, kBodyCount, "vehicle", kUnknown) == kBodyCount - 1);
  CHECK(index_of(kBodies, kBodyCount, "ghost", kUnknown) == kUnknown);
}

TEST_CASE("a mesh lists each of its materials once, in the order its parts use them") {
  MeshDesc mesh;
  mesh.primitives = {part_with(kWood), part_with(kNoIndex), part_with(kMetal), part_with(kWood)};
  CHECK(materials_of(mesh) == std::vector<int>{kWood, kMetal});
  CHECK(materials_of(MeshDesc{}).empty());
}

TEST_CASE("a list without one of its items keeps the others in order") {
  const std::vector<std::string> scripts = {kFirstScript, kMiddleScript, kLastScript};
  CHECK(without_item(scripts, kMiddle) == std::vector<std::string>{kFirstScript, kLastScript});
  CHECK(without_item(scripts, 0) == std::vector<std::string>{kMiddleScript, kLastScript});
  CHECK(without_item({kFirstScript}, 0).empty());
}

TEST_CASE("a vector kept at least so big raises only the parts that are below") {
  CHECK(approx_equal(at_least(kMirroredScale, kSmallestScale), kUsableScale, kTolerance));
  CHECK(approx_equal(at_least(kUsableScale, kSmallestScale), kUsableScale, kTolerance));
}

TEST_CASE("the corners of a box follow the object's placement") {
  Bounds bounds;
  bounds.include(kBoxMin);
  bounds.include(kBoxMax);
  const auto unmoved = box_corners(Transform{}, bounds);
  CHECK(approx_equal(unmoved[0], kBoxMin, kTolerance));
  CHECK(approx_equal(unmoved[kAllMax], kBoxMax, kTolerance));
  CHECK(approx_equal(unmoved[kMaxOnY], Vec3{kBoxMin.x, kBoxMax.y, kBoxMin.z}, kTolerance));

  const Transform world = turned_and_scaled();
  const auto corners = box_corners(world, bounds);
  for (int i = 0; i < kBoxCorners; ++i) CHECK(approx_equal(corners[i], to_world(world, unmoved[i]), kTolerance));
}

TEST_CASE("a model dropped on the Scene view lands on the ground under the mouse, or in front of the camera") {
  CHECK(approx_equal(drop_point(kCameraPlace, kDownAndForward, kSpawn), kHitOnGround, kTolerance));
  CHECK(approx_equal(drop_point(kCameraPlace, kUpwards, kSpawn), kSpawn, kTolerance));
  CHECK(approx_equal(drop_point(kCameraPlace, kAlmostLevel, kSpawn), kSpawn, kTolerance));
}

TEST_CASE("two opposite keys make one fly axis") {
  CHECK(fly_axis(false, true) == 1.0f);
  CHECK(fly_axis(true, false) == -1.0f);
  CHECK(fly_axis(true, true) == 0.0f);
  CHECK(fly_axis(false, false) == 0.0f);
}

TEST_CASE("reversing a brush swaps Raise and Lower and leaves the others") {
  CHECK(reversed(SculptMode::Raise) == SculptMode::Lower);
  CHECK(reversed(SculptMode::Lower) == SculptMode::Raise);
  CHECK(reversed(SculptMode::Smooth) == SculptMode::Smooth);
  CHECK(reversed(SculptMode::Flatten) == SculptMode::Flatten);
}

TEST_CASE("a brush moves the ground by its strength per second, and smooths more gently") {
  CHECK(moves_ground(SculptMode::Raise));
  CHECK(moves_ground(SculptMode::Lower));
  CHECK_FALSE(moves_ground(SculptMode::Smooth));
  CHECK(brush_amount(SculptMode::Raise, kStrength, kHeldSeconds) == doctest::Approx(kRaisedMeters));
  CHECK(brush_amount(SculptMode::Lower, kStrength, kHeldSeconds) == doctest::Approx(kRaisedMeters));
  CHECK(brush_amount(SculptMode::Smooth, kStrength, kHeldSeconds) < kRaisedMeters);
  CHECK(brush_amount(SculptMode::Flatten, kStrength, kHeldSeconds) ==
        doctest::Approx(brush_amount(SculptMode::Smooth, kStrength, kHeldSeconds)));
  CHECK(brush_amount(SculptMode::Raise, kStrength, 0.0f) == 0.0f);
}

TEST_CASE("a point goes into an object's own space and back") {
  const Transform world = turned_and_scaled();
  CHECK(approx_equal(to_local(world, kPlace), Vec3::zero(), kTolerance));
  CHECK(approx_equal(to_world(world, Vec3::zero()), kPlace, kTolerance));
  CHECK(approx_equal(to_world(world, to_local(world, kSpawn)), kSpawn, kTolerance));
}

TEST_CASE("a brush on a scaled terrain is measured in the terrain's own grid") {
  Transform terrain;
  terrain.position = kTerrainPlace;
  terrain.scale = kTerrainScale;
  const TerrainBrush raising = local_brush(terrain, kPointOnTerrain, kBrushRadius, kRaisedMeters, SculptMode::Raise);
  CHECK(raising.x == doctest::Approx(kLocalX));
  CHECK(raising.z == doctest::Approx(kLocalZ));
  CHECK(raising.radius == doctest::Approx(kLocalRadius));
  CHECK(raising.amount == doctest::Approx(kLocalMeters));
  CHECK(raising.mode == SculptMode::Raise);

  // Smoothing is a share, not a height: the terrain's scale does not change it.
  const TerrainBrush smooth = local_brush(terrain, kPointOnTerrain, kBrushRadius, kHeldSeconds, SculptMode::Smooth);
  CHECK(smooth.amount == doctest::Approx(kHeldSeconds));
}
