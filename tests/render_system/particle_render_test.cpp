// System test: particles as the renderer draws them, read back as pixels: the cells of a sprite
// sheet over a particle's life, and soft particles fading where they meet a wall.
#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "render_system/render_stage.hpp"
#include "scene/particles.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr Vec3 kCameraAt{0.0f, 0.0f, 10.0f};
constexpr float kMiddle = 0.5f;
constexpr float kLongLife = 100.0f;  // seconds: no particle of these tests ends while they look at it
constexpr float kWallGlow = 2.0f;
constexpr Vec3 kWallSize{8.0f, 8.0f, 1.0f};
constexpr float kWallFront = 0.5f;    // meters: where the wall's face is along z
constexpr float kJustInFront = 0.2f;  // meters between a particle and the wall's face
constexpr float kWellInFront = 4.0f;
constexpr float kSoftOver = 2.0f;     // meters a soft particle fades over
constexpr float kBigParticle = 3.0f;  // meters across
constexpr int kCellPixels = 32;       // of the sprite sheet: two cells, red then green
constexpr float kEarly = 0.2f;        // shares of the life
constexpr float kLate = 0.7f;
constexpr const char* kSheet = "sheet";
constexpr ::Color kRed{255, 0, 0, 255};
constexpr ::Color kGreen{0, 255, 0, 255};
constexpr redgem::Color kBlue{0.0f, 0.0f, 1.0f, 1.0f};
constexpr redgem::Color kBlack{0.0f, 0.0f, 0.0f, 1.0f};

bool redder_than_blue(::Color c) { return c.r > c.b; }
bool greener_than_red(::Color c) { return c.g > c.r; }

// One still, opaque particle at `place`, of a plain color that does not change over its life.
ParticleBurst still_particle(Vec3 place, Vec3 color) {
  ParticleBurst burst;
  burst.position = place;
  burst.count = 1;
  burst.speed = 0.0f;
  burst.gravity = 0.0f;
  burst.life = kLongLife;
  burst.look.size = steady(kBigParticle);
  burst.look.color = steady(color);
  burst.look.alpha = steady(1.0f);
  return burst;
}

// A camera and a wall of `color` that shines the same whatever the light, its face toward the camera.
SceneDesc wall_scene(redgem::Color color) {
  SceneDesc desc;
  desc.materials = {test::glowing_material(color, kWallGlow)};
  desc.meshes = {test::cube_mesh(0)};
  desc.nodes = {test::camera_at(kCameraAt)};
  NodeDesc wall;
  wall.name = "Wall";
  wall.mesh = 0;
  wall.local.scale = kWallSize;
  desc.nodes.push_back(wall);
  return desc;
}

::Color middle_of(RenderStage& stage, const SceneDesc& desc, const ParticleSystem& particles, const std::string& name) {
  return stage.draw(Scene::from_desc(desc), name, &particles).at(kMiddle, kMiddle);
}

// A sprite sheet of two cells in a row, red then green, in a folder of the test's own.
std::filesystem::path write_sheet() {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-render-sheet";
  std::filesystem::create_directories(dir);
  Image sheet = GenImageColor(kCellPixels * 2, kCellPixels, kRed);
  ImageDrawRectangle(&sheet, kCellPixels, 0, kCellPixels, kCellPixels, kGreen);
  ExportImage(sheet, (dir / (std::string(kSheet) + ".png")).string().c_str());
  UnloadImage(sheet);
  return dir;
}

}  // namespace

TEST_CASE("a soft particle fades where it meets a wall; a hard one, and a soft one well away, do not") {
  RenderStage stage;
  const SceneDesc desc = wall_scene(kBlue);
  stage.upload(desc);
  const Vec3 red{1.0f, 0.0f, 0.0f};
  ParticleSystem hard;
  hard.burst(still_particle({0.0f, 0.0f, kWallFront + kJustInFront}, red));
  CHECK(redder_than_blue(middle_of(stage, desc, hard, "particle_hard_at_wall")));
  ParticleSystem soft;
  ParticleBurst fading = still_particle({0.0f, 0.0f, kWallFront + kJustInFront}, red);
  fading.look.soft = kSoftOver;
  soft.burst(fading);
  CHECK_FALSE(redder_than_blue(middle_of(stage, desc, soft, "particle_soft_at_wall")));
  ParticleSystem away;
  fading.position = {0.0f, 0.0f, kWallFront + kWellInFront};
  away.burst(fading);
  CHECK(redder_than_blue(middle_of(stage, desc, away, "particle_soft_away_from_wall")));
}

TEST_CASE("with several samples a pixel a soft particle fades where it meets a wall as with one") {
  constexpr double kSamples = 4.0;
  RenderStage stage;
  SceneDesc desc = wall_scene(kBlue);
  NodeDesc environment;
  environment.name = "Environment";
  environment.props["environment"] = true;
  environment.props["msaa"] = kSamples;
  desc.nodes.push_back(environment);
  stage.upload(desc);
  ParticleBurst fading = still_particle({0.0f, 0.0f, kWallFront + kJustInFront}, Vec3{1.0f, 0.0f, 0.0f});
  fading.look.soft = kSoftOver;
  ParticleSystem soft;
  soft.burst(fading);
  // (The wall's depth reaches the particles only when the samples' depth is copied out whole.)
  CHECK_FALSE(redder_than_blue(middle_of(stage, desc, soft, "particle_soft_at_wall_msaa")));
}

TEST_CASE("a particle wearing a sprite sheet shows its cells one after the other over its life") {
  RenderStage stage;
  SceneDesc desc;
  desc.nodes = {test::camera_at(kCameraAt)};
  stage.upload(desc);
  stage.set_images_dir(write_sheet());
  ParticleBurst burst = still_particle(Vec3::zero(), Vec3::one());
  burst.image = kSheet;
  burst.look.flipbook = Flipbook{2, 1, 0.0f};  // the two cells once over the life
  burst.spin_degrees = 0.0f;
  ParticleSystem particles;
  particles.burst(burst);
  particles.update(kLongLife * kEarly);
  const ::Color early = middle_of(stage, desc, particles, "particle_flipbook_first_cell");
  particles.update(kLongLife * (kLate - kEarly));
  const ::Color late = middle_of(stage, desc, particles, "particle_flipbook_second_cell");
  CHECK_FALSE(greener_than_red(early));
  CHECK(greener_than_red(late));
}

TEST_CASE("many glowing particles over each other wash out to white, unless the environment limits their glow") {
  constexpr int kCrowd = 60;
  constexpr float kGlowLimit = 1.5f;
  constexpr int kStillOrange = 60;  // red over blue by this much (of 255) is a color, not white
  RenderStage stage;
  SceneDesc desc = wall_scene(kBlack);  // so that the glow alone makes the color
  ParticleBurst flames = still_particle({0.0f, 0.0f, kWallFront + kWellInFront}, Vec3{1.0f, 0.45f, 0.1f});
  flames.count = kCrowd;
  flames.look.glow = true;
  ParticleSystem particles;
  particles.burst(flames);
  stage.upload(desc);
  const ::Color unlimited = middle_of(stage, desc, particles, "particle_glow_unlimited");
  NodeDesc environment;
  environment.name = "Environment";
  environment.props["environment"] = true;
  environment.props["glow_limit"] = static_cast<double>(kGlowLimit);
  desc.nodes.push_back(environment);
  const ::Color limited = middle_of(stage, desc, particles, "particle_glow_limited");
  CHECK(unlimited.r - unlimited.b < kStillOrange);
  CHECK(limited.r - limited.b > kStillOrange);
}
