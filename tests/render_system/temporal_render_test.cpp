// System test: edges smoothed with the frames before (antialias = "temporal"), drawn in a real
// window: the fine pattern of copies fading out turns into an even shade, and what moves (or
// jumps) leaves no trail behind.
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "render/temporal.hpp"
#include "render_system/render_stage.hpp"
#include "scene/shapes/primitives.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr Vec3 kCameraAt{0.0f, 0.0f, 10.0f};
constexpr int kSettleFrames = 16;  // the frames before have settled by then
constexpr int kFadeCopies = 40;
constexpr float kFadeEnd = 12.0f;     // meters from the camera the copies are drawn to
constexpr float kFade = 6.0f;         // meters they fade over before that
constexpr float kRowDistance = 9.0f;  // meters from the camera: half faded
constexpr float kRowSpacing = 0.3f;
// The even shade's pixel to pixel change for its light, at most, of the pattern's.
constexpr float kSmoother = 0.5f;
// The light it keeps of the pattern's, at least. (It keeps more: the frames are mixed in linear
// light, before the picture's tone is mapped, as the light of the pixels' parts would add up.)
constexpr float kKeptLeast = 0.8f;
constexpr float kLeft = -2.0f;    // meters across, where the cube starts
constexpr float kRight = 2.0f;    // and where it jumps to
constexpr float kStride = 0.25f;  // meters it moves a frame, moving on
constexpr int kMovingFrames = 8;
constexpr int kTrace = 24;        // a pixel this green still shows the cube
constexpr int kLooseColumns = 2;  // columns a moving cube may cover beyond its own width
constexpr double kSamples = 4.0;
constexpr const char* kBlack = "#000000";
constexpr float kGlow = 2.0f;  // how many times its color the cube shines

// Grass: cards of thin cut-out blades swaying in the wind, a camera passing over them.
constexpr int kBladesSide = 32;  // texels of the blades' picture
constexpr int kBladeWidth = 3;   // texels: about a pixel and a half, seen from the camera
constexpr int kBladeEvery = 8;   // a blade every this many texels across
constexpr float kBladeCutoff = 0.5f;
constexpr int kCardsAcross = 15;
constexpr int kCardsDeep = 3;
constexpr float kCardSpacing = 0.6f;  // meters
constexpr float kCardSize = 0.4f;
constexpr float kPlaneSide = 10.0f;     // meters across the plane a card is made from
constexpr float kFieldDistance = 6.0f;  // meters from the camera to the field's middle
constexpr Vec3 kFieldCamera{0.0f, 0.8f, 6.0f};
constexpr float kPass = 0.6f;  // meters the camera moves across, over kPassFrames
constexpr int kPassFrames = 12;
constexpr double kWindy = 1.0;
// Smoothed with the frames before while the camera moves, the grass is no farther from how it
// settles with the camera still than this share of how far one sample a pixel is from that: it
// smears no detail away.
constexpr float kNoFartherThanAliased = 1.25f;

// How green a pixel is over the others.
int greenness(::Color c) { return std::max(static_cast<int>(c.g) - std::max(c.r, c.b), 0); }
bool green(::Color c) { return greenness(c) > kTrace; }

// The change from each pixel to the next across, summed.
float roughness(const Picture& picture) {
  float total = 0.0f;
  for (std::size_t i = 0; i + 1 < picture.pixels.size(); ++i) {
    total += static_cast<float>(std::abs(greenness(picture.pixels[i]) - greenness(picture.pixels[i + 1])));
  }
  return total;
}

float total_greenness(const Picture& picture) {
  float total = 0.0f;
  for (const ::Color c : picture.pixels) total += static_cast<float>(greenness(c));
  return total;
}

// How many columns of the picture have a green pixel.
int green_columns(const Picture& picture) {
  int columns = 0;
  for (int x = 0; x < picture.width; ++x) {
    bool found = false;
    for (int y = 0; y < picture.height && !found; ++y) {
      found = green(picture.pixels[static_cast<std::size_t>(y * picture.width + x)]);
    }
    columns += found ? 1 : 0;
  }
  return columns;
}

// A night with nothing in it but what glows: no sky, fog, bloom or grade to tell apart from a trail.
NodeDesc dark_environment(PropertyMap props) {
  NodeDesc node;
  node.name = "Environment";
  node.props = std::move(props);
  for (const char* color : {"sky_top", "sky_horizon", "ground_color", "fog_color"})
    node.props[color] = std::string(kBlack);
  for (const char* off : {"fog_density", "ambient", "bloom", "vignette", "sun_glow", "grain", "ambient_occlusion"}) {
    node.props[off] = 0.0;
  }
  node.props["environment"] = true;
  return node;
}

PropertyMap temporal(PropertyMap props = {}) {
  props["antialias"] = std::string("temporal");
  return props;
}

// A row of glowing cubes across the view, half faded: a fine pattern of their pixels is left out.
Scene fading_row(RenderStage& stage, PropertyMap environment) {
  SceneDesc desc;
  desc.materials = {test::glowing_material(redgem::Color{0.0f, 1.0f, 0.0f, 1.0f}, kGlow)};
  desc.meshes = {test::cube_mesh(0)};
  desc.nodes = {test::camera_at(kCameraAt), dark_environment(std::move(environment))};
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  ScatterSet set;
  set.mesh = 0;
  set.visible_to = kFadeEnd;
  set.fade = kFade;
  for (int i = 0; i < kFadeCopies; ++i) {
    const float x = (static_cast<float>(i) - static_cast<float>(kFadeCopies) * kHalf) * kRowSpacing;
    set.places.push_back(redgem::Transform{{x, 0.0f, kCameraAt.z - kRowDistance}, {}, Vec3::one() * kRowSpacing});
  }
  scene.scattered().add(set);
  return scene;
}

// One glowing cube, kLeft across.
SceneDesc glowing_cube(PropertyMap environment) {
  SceneDesc desc;
  desc.materials = {test::glowing_material(redgem::Color{0.0f, 1.0f, 0.0f, 1.0f}, kGlow)};
  desc.meshes = {test::cube_mesh(0)};
  NodeDesc cube;
  cube.name = "Cube";
  cube.mesh = 0;
  cube.local.position = {kLeft, 0.0f, 0.0f};
  desc.nodes = {test::camera_at(kCameraAt), cube, dark_environment(std::move(environment))};
  return desc;
}

Picture settle(RenderStage& stage, const Scene& scene, const std::string& name) {
  for (int frame = 1; frame < kSettleFrames; ++frame) stage.draw(scene, name);
  return stage.draw(scene, name);
}

// How many pixels of the left half of the picture are green.
std::size_t green_on_the_left(const Picture& picture) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < picture.pixels.size(); ++i) {
    const bool left = static_cast<int>(i % static_cast<std::size_t>(picture.width)) < picture.width / 2;
    if (left && green(picture.pixels[i])) ++count;
  }
  return count;
}

// A picture of thin green blades on nothing, as a PNG.
ImageData blades_picture() {
  Image image = GenImageColor(kBladesSide, kBladesSide, ::Color{0, 0, 0, 0});
  for (int x = 0; x < kBladesSide; x += kBladeEvery) {
    ImageDrawRectangle(&image, x, 0, kBladeWidth, kBladesSide, ::Color{60, 200, 40, 255});
  }
  int size = 0;
  unsigned char* png = ExportImageToMemory(image, ".png", &size);
  ImageData data{std::vector<std::uint8_t>(png, png + size), ".png"};
  MemFree(png);
  UnloadImage(image);
  return data;
}

SceneDesc grass_field(PropertyMap environment) {
  SceneDesc desc;
  desc.images = {blades_picture()};
  MaterialDesc blades;
  blades.base_color_image = 0;
  blades.alpha_mode = AlphaMode::Mask;
  blades.alpha_cutoff = kBladeCutoff;
  desc.materials = {blades};
  desc.meshes = {test::cube_mesh(0)};
  desc.meshes[0].primitives[0] = make_primitive(PrimitiveShape::Plane);
  desc.meshes[0].primitives[0].material = 0;
  environment["wind"] = kWindy;
  environment["ambient"] = 1.0;
  desc.nodes = {test::camera_at(kFieldCamera), dark_environment(std::move(environment))};
  desc.nodes[1].props["ambient"] = 1.0;
  return desc;
}

// The field's cards, standing up across the view.
ScatterSet grass_cards() {
  ScatterSet set;
  set.mesh = 0;
  const Quat standing = quat_from_axis_angle(Vec3{1.0f, 0.0f, 0.0f}, kPi * kHalf);
  for (int row = 0; row < kCardsDeep; ++row) {
    for (int column = 0; column < kCardsAcross; ++column) {
      const float x = (static_cast<float>(column) - static_cast<float>(kCardsAcross) * kHalf) * kCardSpacing;
      const float z = kFieldCamera.z - kFieldDistance + (static_cast<float>(row) - kCardsDeep * kHalf) * kCardSpacing;
      set.places.push_back(redgem::Transform{{x, 0.0f, z}, standing, Vec3::one() * (kCardSize / kPlaneSide)});
    }
  }
  return set;
}

// The field as the camera sees it at the end of its pass, with `environment`, after `pass_frames`
// frames on its way there, `pass` meters across.
Picture over_the_grass(RenderStage& stage, PropertyMap environment, float pass, int pass_frames,
                       const std::string& name) {
  const SceneDesc desc = grass_field(std::move(environment));
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  scene.scattered().add(grass_cards());
  const ObjectId camera = scene.find("Camera");
  Picture picture;
  for (int frame = pass_frames; frame >= 0; --frame) {
    const float step = pass * static_cast<float>(frame) / static_cast<float>(kPassFrames);
    scene.set_world_position(camera, kFieldCamera - Vec3{step, 0.0f, 0.0f});
    picture = stage.draw(scene, name);
  }
  return picture;
}

// How far one picture is from another: their channels' differences, summed.
float distance_between(const Picture& a, const Picture& b) {
  float total = 0.0f;
  for (std::size_t i = 0; i < a.pixels.size(); ++i) {
    total += static_cast<float>(std::abs(a.pixels[i].r - b.pixels[i].r) + std::abs(a.pixels[i].g - b.pixels[i].g) +
                                std::abs(a.pixels[i].b - b.pixels[i].b));
  }
  return total;
}

}  // namespace

TEST_CASE("smoothed with the frames before, the pattern of copies fading out turns into an even shade") {
  RenderStage stage;
  const Picture pattern = stage.draw(fading_row(stage, {}), "temporal_fade_pattern");
  const Picture shade = settle(stage, fading_row(stage, temporal()), "temporal_fade_shade");
  REQUIRE(total_greenness(pattern) > 0.0f);
  REQUIRE(total_greenness(shade) > 0.0f);
  const float pattern_light = total_greenness(pattern);
  const float shade_light = total_greenness(shade);
  CHECK(roughness(shade) / shade_light < roughness(pattern) / pattern_light * kSmoother);
  CHECK(shade_light > pattern_light * kKeptLeast);
}

TEST_CASE("smoothed with the frames before, a cube that jumps or moves leaves no trail, with or without msaa") {
  for (const double samples : {0.0, kSamples}) {
    RenderStage stage;
    const SceneDesc desc = glowing_cube(temporal({{"msaa", samples}}));
    stage.upload(desc);
    Scene scene = Scene::from_desc(desc);
    const ObjectId cube = scene.find("Cube");
    const int still_width = green_columns(settle(stage, scene, "temporal_cube_still"));
    REQUIRE(still_width > 0);
    scene.set_world_position(cube, {kRight, 0.0f, 0.0f});
    const Picture jumped = stage.draw(scene, "temporal_cube_jumped");
    CHECK(green_on_the_left(jumped) == 0);
    CHECK(green_columns(jumped) <= still_width + kLooseColumns);
    Picture moving = jumped;
    for (int frame = 1; frame <= kMovingFrames; ++frame) {
      scene.set_world_position(cube, {kRight - kStride * static_cast<float>(frame), 0.0f, 0.0f});
      moving = stage.draw(scene, "temporal_cube_moving");
    }
    CHECK(green_columns(moving) <= still_width + kLooseColumns);
  }
}

TEST_CASE("smoothed with the frames before, swaying grass passed over looks as it does with the camera still") {
  RenderStage stage;
  const Picture passing = over_the_grass(stage, temporal(), kPass, kPassFrames, "temporal_grass_passing");
  // Right after (the wind has hardly moved): the same view, settled with the camera still, and
  // with one sample a pixel.
  const Picture still = over_the_grass(stage, temporal(), 0.0f, kPassFrames, "temporal_grass_still");
  const Picture aliased = over_the_grass(stage, {}, 0.0f, 0, "temporal_grass_aliased");
  const float passing_error = distance_between(passing, still);
  const float aliased_error = distance_between(aliased, still);
  MESSAGE("passing " << passing_error << ", aliased " << aliased_error);
  REQUIRE(aliased_error > 0.0f);
  CHECK(passing_error < aliased_error * kNoFartherThanAliased);
  CHECK(total_greenness(passing) > total_greenness(aliased) * kKeptLeast);  // no blade fades away
}

namespace {

constexpr int kChaseFrames = render_math::kJitterPhases;  // final frame shares the first frame's jitter
constexpr Vec3 kChaseStep{1.25f, 0.0f, -0.25f};           // over 32 pixels/frame on the wall, below camera-cut distance
constexpr Vec3 kChaseWallSize{100.0f, 12.0f, 1.0f};
constexpr Vec3 kChaseCarOffset{0.0f, -1.0f, -4.0f};  // a tracked subject with almost no screen motion
constexpr Vec3 kChaseCarSize{1.4f, 0.7f, 2.0f};
constexpr int kChaseTextureWidth = 4096;  // thin stripes at about one pixel, seen at the final view
constexpr int kChaseTextureHeight = 8;
constexpr int kChaseStripeWidth = 2;
constexpr ::Color kChaseStripeColor{0, 255, 0, 255};
constexpr ::Color kChaseStripeBackground{0, 0, 0, 255};  // dark gaps expose lost contrast
constexpr float kChaseRegionLeft = 0.15f;                // wall detail only, above the tracked subject
constexpr float kChaseRegionRight = 0.85f;
constexpr float kChaseRegionTop = 0.15f;
constexpr float kChaseRegionBottom = 0.40f;
constexpr float kChaseMeanError = 0.5f;  // byte mean absolute error against a fresh, same-jitter frame

ImageData chase_stripes() {
  Image image = GenImageColor(kChaseTextureWidth, kChaseTextureHeight, kChaseStripeBackground);
  for (int x = 0; x < kChaseTextureWidth; x += kChaseStripeWidth * 2)
    ImageDrawRectangle(&image, x, 0, kChaseStripeWidth, kChaseTextureHeight, kChaseStripeColor);
  int size = 0;
  unsigned char* png = ExportImageToMemory(image, ".png", &size);
  ImageData data{std::vector<std::uint8_t>(png, png + size), ".png"};
  MemFree(png);
  UnloadImage(image);
  return data;
}

NodeDesc chase_wall() {
  NodeDesc wall;
  wall.name = "Wall";
  wall.mesh = 0;
  wall.local.scale = kChaseWallSize;
  return wall;
}

NodeDesc chase_car() {
  NodeDesc car;
  car.name = "Car";
  car.mesh = 1;
  car.local.position = kCameraAt + kChaseCarOffset;
  car.local.scale = kChaseCarSize;
  return car;
}

SceneDesc chase_scene(PropertyMap environment) {
  SceneDesc desc;
  desc.images = {chase_stripes()};
  MaterialDesc wall = test::glowing_material(redgem::Color{0.0f, 0.0f, 0.0f, 1.0f}, 0.0f);
  wall.emissive = Vec3::one() * kGlow;
  wall.emissive_image = 0;
  desc.materials = {wall, test::glowing_material(redgem::Color{0.0f, 0.0f, 1.0f, 1.0f}, kGlow)};
  desc.meshes = {test::cube_mesh(0), test::cube_mesh(1)};
  desc.nodes = {test::camera_at(kCameraAt), chase_wall(), chase_car(), dark_environment(std::move(environment))};
  return desc;
}

// Both camera and car travel together. The roadside wall moves quickly, the tracked car stays
// in place on screen, and no frame crosses the global camera-cut threshold.
Picture chase_picture(PropertyMap environment, bool moving, const std::string& name) {
  RenderStage stage;
  stage.set_clock(0.0);
  const SceneDesc desc = chase_scene(std::move(environment));
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  Picture picture;
  for (int frame = moving ? kChaseFrames : 0; frame >= 0; --frame) {
    const Vec3 camera = kCameraAt - kChaseStep * static_cast<float>(frame);
    scene.set_world_position(scene.find("Camera"), camera);
    scene.set_world_position(scene.find("Car"), camera + kChaseCarOffset);
    picture = stage.draw(scene, name);
  }
  return picture;
}

bool in_chase_detail_region(int x, int y, const Picture& picture) {
  const float across = static_cast<float>(x) / static_cast<float>(picture.width);
  const float down = static_cast<float>(y) / static_cast<float>(picture.height);
  return across > kChaseRegionLeft && across < kChaseRegionRight && down > kChaseRegionTop && down < kChaseRegionBottom;
}

float chase_pixel_error(::Color a, ::Color b) {
  return static_cast<float>(std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b));
}

float chase_detail_error(const Picture& moving, const Picture& fresh) {
  float error = 0.0f;
  std::size_t pixels = 0;
  for (int y = 0; y < moving.height; ++y) {
    for (int x = 0; x < moving.width; ++x) {
      if (!in_chase_detail_region(x, y, moving)) continue;
      const auto i = static_cast<std::size_t>(y * moving.width + x);
      error += chase_pixel_error(moving.pixels[i], fresh.pixels[i]);
      ++pixels;
    }
  }
  return error / static_cast<float>(pixels);
}

}  // namespace

TEST_CASE("a fast chase camera retains current roadside detail without discarding history for the whole scene") {
  for (const double samples : {0.0, kSamples}) {
    const PropertyMap environment = temporal({{"msaa", samples}});
    const Picture moving = chase_picture(environment, true, "temporal_chase_fast");
    const Picture fresh = chase_picture(environment, false, "temporal_chase_fresh");
    REQUIRE(total_greenness(fresh) > 0.0f);
    const float error = chase_detail_error(moving, fresh);
    MESSAGE("fast chase detail error " << error << ", samples " << samples);
    CHECK(error < kChaseMeanError);
  }
}

TEST_CASE("a chase camera with temporal antialiasing off produces the same final pixels as a fresh view") {
  const Picture moving = chase_picture({}, true, "temporal_chase_off_moving");
  const Picture fresh = chase_picture({}, false, "temporal_chase_off_fresh");
  REQUIRE(moving.pixels.size() == fresh.pixels.size());
  CHECK(distance_between(moving, fresh) == 0.0f);
}

namespace {

// At 256 pixels and 45 degrees, this step moves the near card about 7.7 pixels and the wall
// about 1.6 pixels. Their split exceeds four pixels; neither reaches the fast-motion ramp.
constexpr float kOpeningCameraStep = 0.05f;
constexpr Vec3 kOpeningCardAt{0.0f, 0.0f, 8.0f};
constexpr Vec3 kOpeningCardSize{0.8f, 1.2f, 0.01f};
constexpr Vec3 kOpeningWhitePatchAt{12.0f, 0.0f, 0.01f};  // its left edge is x=2, just beyond the card's projection
constexpr Vec3 kOpeningWhitePatchSize{20.0f, 12.0f, 1.0f};
constexpr redgem::Color kOpeningWhite{1.0f, 1.0f, 1.0f, 1.0f};
constexpr redgem::Color kOpeningBlack{0.0f, 0.0f, 0.0f, 1.0f};
constexpr int kOpeningBlackCeiling = 5;    // byte brightness of a freshly revealed black pixel
constexpr int kOpeningWhiteFloor = 100;    // byte brightness of its opaque white neighbor
constexpr float kOpeningRegionTop = 0.4f;  // central card rows, excluding its horizontal edges
constexpr float kOpeningRegionBottom = 0.6f;
constexpr std::size_t kOpeningMinimumPixels = 30;  // the edge assertion cannot pass with an empty selection
constexpr float kOpeningMeanError = 0.5f;          // mean RGB byte error against the same-jitter fresh view

NodeDesc opening_cube(const std::string& name, int mesh, Vec3 position, Vec3 scale) {
  NodeDesc node;
  node.name = name;
  node.mesh = mesh;
  node.local.position = position;
  node.local.scale = scale;
  return node;
}

NodeDesc opening_dark_sun() {
  NodeDesc sun;
  sun.name = "Sun";
  sun.light = LightDesc{};
  sun.light->intensity = 0.0f;  // emissive black and white alone define the test's signal
  return sun;
}

SceneDesc opening_scene() {
  SceneDesc desc;
  desc.materials = {test::glowing_material(kOpeningBlack, 0.0f), test::glowing_material(kOpeningWhite, kGlow)};
  desc.meshes = {test::cube_mesh(0), test::cube_mesh(1)};
  desc.nodes = {test::camera_at(kCameraAt),
                opening_dark_sun(),
                dark_environment(temporal()),
                opening_cube("Wall", 0, Vec3::zero(), kChaseWallSize),
                opening_cube("Card", 1, kOpeningCardAt, kOpeningCardSize),
                opening_cube("WhitePatch", 1, kOpeningWhitePatchAt, kOpeningWhitePatchSize)};
  return desc;
}

// The warmed run moves only on its final frame. Eight warm-up frames put that last frame in
// the same jitter phase as a freshly uploaded reference, without invalidating global history.
Picture opening_picture(bool moving, const std::string& name) {
  RenderStage stage;
  stage.set_clock(0.0);
  const SceneDesc desc = opening_scene();
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  Picture picture;
  for (int frame = 0; frame <= (moving ? render_math::kJitterPhases : 0); ++frame) {
    const bool before_step = moving && frame < render_math::kJitterPhases;
    const Vec3 camera = kCameraAt - Vec3{before_step ? kOpeningCameraStep : 0.0f, 0.0f, 0.0f};
    scene.set_world_position(scene.find("Camera"), camera);
    picture = stage.draw(scene, name);
  }
  return picture;
}

bool opening_edge_colors(::Color current, ::Color left) {
  const bool black = std::max({current.r, current.g, current.b}) < kOpeningBlackCeiling;
  const bool white = std::min({left.r, left.g, left.b}) > kOpeningWhiteFloor;
  return black && white;
}

bool opening_edge_pixel(const Picture& fresh, int x, int y) {
  const float down = static_cast<float>(y) / static_cast<float>(fresh.height);
  const bool central_right = x > fresh.width / 2 && down > kOpeningRegionTop && down < kOpeningRegionBottom;
  if (!central_right) return false;
  const auto i = static_cast<std::size_t>(y * fresh.width + x);
  return opening_edge_colors(fresh.pixels[i], fresh.pixels[i - 1]);
}

struct OpeningError {
  std::size_t pixels = 0;
  float total = 0.0f;
};

OpeningError opening_error(const Picture& moving, const Picture& fresh) {
  OpeningError error;
  for (int y = 0; y < fresh.height; ++y) {
    for (int x = 1; x < fresh.width; ++x) {
      if (!opening_edge_pixel(fresh, x, y)) continue;
      const auto i = static_cast<std::size_t>(y * fresh.width + x);
      error.total += chase_pixel_error(moving.pixels[i], fresh.pixels[i]);
      ++error.pixels;
    }
  }
  return error;
}

}  // namespace

TEST_CASE("an opening edge rejects foreground history even below the fast-motion threshold") {
  const Picture moving = opening_picture(true, "temporal_opening_moved");
  const Picture fresh = opening_picture(false, "temporal_opening_fresh");
  REQUIRE(moving.pixels.size() == fresh.pixels.size());
  const OpeningError error = opening_error(moving, fresh);
  REQUIRE(error.pixels >= kOpeningMinimumPixels);
  const float mean = error.total / static_cast<float>(error.pixels);
  MESSAGE("opening edge pixels " << error.pixels << ", mean error " << mean);
  CHECK(mean < kOpeningMeanError);
}

namespace {

// A stopped foreground object disappears without leaving a velocity discontinuity. The dark
// background edge still has a bright neighbor, so color clipping alone cannot reject its history.
constexpr float kDepthBackgroundEdge = 0.1f;  // keep the exposed black edge just right of the picture center

void add_depth_skin(SceneDesc& desc) {
  SkinDesc skin;
  skin.joints = {static_cast<int>(desc.nodes.size())};
  NodeDesc joint;
  joint.name = "DepthJoint";
  joint.local = desc.nodes[4].local;  // a skinned vertex follows the joint, not the mesh node
  desc.nodes.push_back(joint);
  skin.inverse_bind = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  desc.skins.push_back(std::move(skin));
  desc.nodes[4].skin = 0;
}

SceneDesc disappearing_scene(double samples, bool skinned) {
  SceneDesc desc = opening_scene();
  desc.nodes[5].local.position.x = kDepthBackgroundEdge - kOpeningWhitePatchSize.x * 0.5f;
  desc.nodes[2].props["msaa"] = samples;
  if (skinned) add_depth_skin(desc);
  return desc;
}

void skin_disappearing_card(SceneDesc& desc) {
  PrimitiveDesc& card = desc.meshes[1].primitives[0];
  for (std::size_t vertex = 0; vertex < card.positions.size() / 3; ++vertex) {
    card.joints.insert(card.joints.end(), {0, 0, 0, 0});
    card.weights.insert(card.weights.end(), {1, 0, 0, 0});
  }
}

Picture disappearing_picture(bool warmed, double samples, bool skinned, const std::string& name) {
  RenderStage stage;
  stage.set_clock(0.0);
  SceneDesc desc = disappearing_scene(samples, skinned);
  if (skinned) skin_disappearing_card(desc);
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  for (int frame = 0; frame < (warmed ? render_math::kJitterPhases : 0); ++frame) {
    stage.draw(scene, name + "_warm");
  }
  scene.at(scene.find("Card")).visible = false;
  return stage.draw(scene, name);
}

}  // namespace

TEST_CASE("temporal depth validation rejects stopped foreground history after an object disappears") {
  for (const double samples : {0.0, kSamples}) {
    for (const bool skinned : {false, true}) {
      const Picture moved = disappearing_picture(true, samples, skinned, "temporal_depth_disappeared");
      const Picture fresh = disappearing_picture(false, samples, skinned, "temporal_depth_disappeared_fresh");
      const OpeningError error = opening_error(moved, fresh);
      REQUIRE(error.pixels >= kOpeningMinimumPixels);
      CHECK(error.total / static_cast<float>(error.pixels) < kOpeningMeanError);
    }
  }
}

namespace {

constexpr float kDepthApproachStep = 0.25f;  // large relative depth change, small screen velocity
constexpr float kDepthCoverage = 0.5f;
constexpr float kDepthStableRoughness = 0.7f;  // accumulated dither must be visibly smoother than one sample
constexpr int kDepthApproachFrames = 8;
constexpr float kDepthInteriorLow = 0.47f;  // avoid the cube's moving silhouette
constexpr float kDepthInteriorHigh = 0.53f;

SceneDesc approaching_depth_scene(double samples, bool skinned) {
  SceneDesc desc = disappearing_scene(samples, skinned);
  desc.materials[1] = test::glowing_material({0, 1, 0, kDepthCoverage}, kGlow);
  desc.materials[1].dithered = true;
  desc.materials[1].alpha_mode = AlphaMode::Blend;
  desc.nodes[5].mesh = kNoIndex;
  desc.nodes[3].mesh = kNoIndex;  // sky behind the dither isolates coverage accumulation
  desc.nodes[4].local.position = Vec3::zero();
  desc.nodes[4].local.scale = Vec3::one();
  if (skinned) {
    skin_disappearing_card(desc);
    desc.nodes.back().local = {};
  }
  return desc;
}

Picture approaching_depth_picture(bool warmed, double samples, bool skinned, const std::string& name) {
  RenderStage stage;
  stage.set_clock(0.0);
  const SceneDesc desc = approaching_depth_scene(samples, skinned);
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  const ObjectId moving = scene.find(skinned ? "DepthJoint" : "Card");
  const int last = kSettleFrames + kDepthApproachFrames;
  Picture picture;
  for (int frame = warmed ? 0 : last; frame <= last; ++frame) {
    const float advance = static_cast<float>(std::max(frame - kSettleFrames, 0)) * kDepthApproachStep;
    scene.set_world_position(moving, Vec3{0, 0, advance});
    picture = stage.draw(scene, name);
  }
  return picture;
}

float depth_interior_roughness(const Picture& picture) {
  float total = 0;
  const int low = static_cast<int>(kDepthInteriorLow * static_cast<float>(picture.width));
  const int high = static_cast<int>(kDepthInteriorHigh * static_cast<float>(picture.width));
  for (int y = low; y < high; ++y) {
    for (int x = low; x < high; ++x) {
      const auto pixel = static_cast<std::size_t>(y * picture.width + x);
      total += static_cast<float>(std::abs(greenness(picture.pixels[pixel]) - greenness(picture.pixels[pixel + 1])));
    }
  }
  return total;
}

}  // namespace

TEST_CASE("temporal depth validation retains dither history while rigid and skinned surfaces approach the camera") {
  for (const double samples : {0.0, kSamples}) {
    for (const bool skinned : {false, true}) {
      const Picture accumulated = approaching_depth_picture(true, samples, skinned, "temporal_depth_approach");
      const Picture fresh = approaching_depth_picture(false, samples, skinned, "temporal_depth_approach_fresh");
      const float aliased = depth_interior_roughness(fresh);
      REQUIRE(aliased > 0.0f);
      CHECK(depth_interior_roughness(accumulated) < aliased * kDepthStableRoughness);
    }
  }
}

namespace {

// Keep the surviving near edge just right of the center ROI. Its subpixel placement leaves
// an opaque motion texel beside the first black receiver texel with both one and four samples.
constexpr float kAdjacentSurvivorEdge = 0.012f;  // world meters, about 1.86 pixels at this fixture's near depth

SceneDesc adjacent_survivor_scene(double samples) {
  SceneDesc desc = opening_scene();
  desc.nodes[2].props["msaa"] = samples;
  desc.nodes[5].mesh = kNoIndex;  // only the persistent near card supplies the bright neighbor
  const float half_width = kOpeningCardSize.x * kHalf;
  desc.nodes[4].local.position.x = kAdjacentSurvivorEdge + half_width;
  const Vec3 survivor_at{kAdjacentSurvivorEdge - half_width, kOpeningCardAt.y, kOpeningCardAt.z};
  desc.nodes.push_back(opening_cube("Survivor", 1, survivor_at, kOpeningCardSize));
  return desc;
}

Picture adjacent_survivor_picture(bool warmed, double samples, const std::string& name) {
  RenderStage stage;
  stage.set_clock(0.0);
  const SceneDesc desc = adjacent_survivor_scene(samples);
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  for (int frame = 0; frame < (warmed ? render_math::kJitterPhases : 0); ++frame) stage.draw(scene, name + "_warm");
  scene.at(scene.find("Card")).visible = false;
  return stage.draw(scene, name);
}

}  // namespace

TEST_CASE("temporal depth validation rejects a removed opaque card beside a surviving near neighbor") {
  for (const double samples : {0.0, kSamples}) {
    const Picture removed = adjacent_survivor_picture(true, samples, "temporal_depth_adjacent_removed");
    const Picture fresh = adjacent_survivor_picture(false, samples, "temporal_depth_adjacent_fresh");
    const OpeningError error = opening_error(removed, fresh);
    REQUIRE(error.pixels >= kOpeningMinimumPixels);
    const float mean = error.total / static_cast<float>(error.pixels);
    MESSAGE("adjacent survivor pixels " << error.pixels << ", mean error " << mean << ", samples " << samples);
    CHECK(mean < kOpeningMeanError);
  }
}

namespace {

// A still glowing cube turned about the view axis: every edge it shows is a staircase of pixels.
constexpr float kTurnedCubeAngle = 0.5f;          // radians about the view axis
constexpr float kTurnedCubeSize = 3.0f;           // meters across: about ninety pixels from the camera
constexpr Vec3 kTurnedWallAt{0.0f, 0.0f, -6.0f};  // a dark wall far enough behind to be another surface
constexpr Vec3 kTurnedWallSize{40.0f, 40.0f, 1.0f};
constexpr float kDimGlow = 0.3f;  // dim enough that the tone curve keeps partial coverage apart from full
constexpr int kStillFrames = 4 * render_math::kJitterPhases;  // long enough to settle, then to compare
constexpr float kInteriorShare = 0.9f;  // of the brightest green: the cube's inside, fully covered
constexpr float kPartlyLow = 0.15f;     // of the inside's green: a pixel only partly covered, from here
constexpr float kPartlyHigh = 0.85f;    // to here
// Partly covered pixels a settled silhouette shows beyond a single sample's, at least: with the
// frames before rejected wherever the previous sample hit the other surface, it shows about 110.
constexpr int kSoftenedLeast = 130;
// Byte change, per cube pixel, from one settled frame to the next and between two frames a jitter
// cycle apart: a picture that holds still and has converged. (Read where the jitter moved it, the
// history follows the jitter: about 6. Rejected at the silhouette: about 1.)
constexpr float kStillFrameChange = 0.7f;
constexpr float kConvergedChange = 0.25f;

SceneDesc turned_cube(PropertyMap environment, bool wall) {
  SceneDesc desc = glowing_cube(std::move(environment));
  desc.materials[0] = test::glowing_material(redgem::Color{0.0f, 1.0f, 0.0f, 1.0f}, kDimGlow);
  desc.materials.push_back(test::glowing_material(kOpeningBlack, 0.0f));
  desc.meshes.push_back(test::cube_mesh(1));
  NodeDesc& cube = desc.nodes[1];
  cube.local.position = Vec3::zero();
  cube.local.rotation = quat_from_axis_angle(Vec3{0.0f, 0.0f, 1.0f}, kTurnedCubeAngle);
  cube.local.scale = Vec3::one() * kTurnedCubeSize;
  if (wall) desc.nodes.push_back(opening_cube("Wall", 1, kTurnedWallAt, kTurnedWallSize));
  return desc;
}

std::vector<Picture> still_frames(PropertyMap environment, bool wall, int frames, const std::string& name) {
  RenderStage stage;
  stage.set_clock(0.0);
  const SceneDesc desc = turned_cube(std::move(environment), wall);
  stage.upload(desc);
  const Scene scene = Scene::from_desc(desc);
  std::vector<Picture> pictures;
  for (int frame = 0; frame < frames; ++frame) pictures.push_back(stage.draw(scene, name));
  return pictures;
}

int brightest_green(const Picture& picture) {
  int brightest = 0;
  for (const ::Color c : picture.pixels) brightest = std::max(brightest, greenness(c));
  return brightest;
}

bool partly_covered(int green_of, int inside) {
  const float share = static_cast<float>(green_of) / static_cast<float>(inside);
  return share > kPartlyLow && share < kPartlyHigh;
}

// How many pixels show the cube only partly: its smoothed silhouette.
int partly_covered_pixels(const Picture& picture) {
  const auto inside = static_cast<int>(static_cast<float>(brightest_green(picture)) * kInteriorShare);
  int count = 0;
  for (const ::Color c : picture.pixels) count += partly_covered(greenness(c), inside) ? 1 : 0;
  return count;
}

// How far `b` is from `a`, in byte change per pixel the cube covers.
float change_per_cube_pixel(const Picture& a, const Picture& b) {
  const auto covered = static_cast<float>(std::count_if(a.pixels.begin(), a.pixels.end(), green));
  return distance_between(a, b) / std::max(covered, 1.0f);
}

}  // namespace

TEST_CASE("smoothed with the frames before, a still view holds still and converges instead of following the jitter") {
  for (const bool wall : {false, true}) {
    const std::vector<Picture> frames = still_frames(temporal(), wall, kStillFrames, "temporal_still_cube");
    const Picture& last = frames.back();
    REQUIRE(total_greenness(last) > 0.0f);
    float most = 0.0f;
    for (std::size_t frame = frames.size() - render_math::kJitterPhases; frame < frames.size(); ++frame) {
      most = std::max(most, change_per_cube_pixel(frames[frame - 1], frames[frame]));
    }
    const float cycle = change_per_cube_pixel(frames[frames.size() - 1 - render_math::kJitterPhases], last);
    MESSAGE("wall " << wall << ": frame to frame " << most << ", a cycle apart " << cycle);
    CHECK(most < kStillFrameChange);
    CHECK(cycle < kConvergedChange);
  }
}

// The first frame has no frames before: one jittered sample a pixel, with no other smoothing.
TEST_CASE("smoothed with the frames before, a still cube's silhouette against the sky or a wall is softened") {
  for (const bool wall : {false, true}) {
    const std::vector<Picture> frames = still_frames(temporal(), wall, kStillFrames, "temporal_silhouette");
    const int hard = partly_covered_pixels(frames.front());
    const int soft = partly_covered_pixels(frames.back());
    MESSAGE("wall " << wall << ": partly covered pixels " << hard << " one sample, " << soft << " smoothed");
    CHECK(soft > hard + kSoftenedLeast);
  }
}

namespace {

// A thin pole and the turned cube, each drifting slowly across the view: under a pixel a frame.
constexpr Vec3 kPoleSize{0.05f, 4.0f, 0.05f};  // meters: about a pixel and a half wide from the camera
constexpr float kSlowStride = 0.01f;           // meters a frame: about a third of a pixel
constexpr int kDriftFrames = 4 * render_math::kJitterPhases;
constexpr int kPoleLooseColumns = 2;  // columns the drifting pole may cover beyond the still one's
// Partly covered pixels the drifting cube's silhouette keeps beyond a single sample's, at least
// (it keeps over 200; with no history at its edges, none).
constexpr int kDriftSoftenedLeast = 150;

SceneDesc pole_scene() {
  SceneDesc desc = glowing_cube(temporal());
  desc.nodes[1].local.position = Vec3::zero();
  desc.nodes[1].local.scale = kPoleSize;
  return desc;
}

// The last of `frames` pictures of `desc`'s node 1 drifting right by `stride` meters a frame.
Picture drifted(const SceneDesc& desc, float stride, int frames, const std::string& name) {
  RenderStage stage;
  stage.set_clock(0.0);
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  const ObjectId moving = scene.find("Cube");
  const Vec3 start = desc.nodes[1].local.position;
  Picture picture;
  for (int frame = 0; frame < frames; ++frame) {
    scene.set_world_position(moving, start + Vec3{stride * static_cast<float>(frame), 0.0f, 0.0f});
    picture = stage.draw(scene, name);
  }
  return picture;
}

}  // namespace

TEST_CASE("smoothed with the frames before, a thin pole drifting slowly in front of the sky leaves no trail") {
  const int still = green_columns(drifted(pole_scene(), 0.0f, kDriftFrames, "temporal_pole_still"));
  REQUIRE(still > 0);
  const int drifting = green_columns(drifted(pole_scene(), kSlowStride, kDriftFrames, "temporal_pole_drifting"));
  MESSAGE("pole columns: still " << still << ", drifting " << drifting);
  CHECK(drifting <= still + kPoleLooseColumns);
}

TEST_CASE("smoothed with the frames before, a slowly moving cube's silhouette keeps its history") {
  for (const bool wall : {false, true}) {
    const SceneDesc desc = turned_cube(temporal(), wall);
    const int hard = partly_covered_pixels(drifted(desc, kSlowStride, 1, "temporal_drift_one_sample"));
    const int soft = partly_covered_pixels(drifted(desc, kSlowStride, kDriftFrames, "temporal_drift_smoothed"));
    MESSAGE("wall " << wall << ": partly covered pixels " << hard << " one sample, " << soft << " drifting");
    CHECK(soft > hard + kDriftSoftenedLeast);
  }
}
