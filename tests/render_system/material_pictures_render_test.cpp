// A material given a picture, another one, or none after the scene was uploaded (as the editor's
// texture slots do) draws with what it has now.

#include <doctest/doctest.h>

#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "render_system/render_stage.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr std::size_t kVisiblePixels = 100;
constexpr unsigned char kBright = 150;
constexpr unsigned char kDark = 70;
constexpr Vec3 kCamera{0, 0, 3};
constexpr redgem::Color kWhite{1, 1, 1, 1};
constexpr float kGlow = 1.0f;

// The pink and green of the lossless WebP fixture.
bool pink(::Color c) { return c.r > kBright && c.b > kBright && c.g < kDark; }
bool green(::Color c) { return c.g > kBright && c.r < kDark && c.b < kDark; }

ImageData picture(const char* file, const char* extension) {
  const auto bytes = read_file_up_to(test::fixture("images") + "/" + file, kMaxEncodedImageBytes);
  REQUIRE(bytes.has_value());
  return ImageData{*bytes, extension};
}

// A white cube that shines with its own color, so a picture on it shows as it is.
SceneDesc plain_cube() {
  SceneDesc desc;
  desc.materials = {test::glowing_material(kWhite, kGlow)};
  desc.meshes = {test::cube_mesh(0)};
  NodeDesc cube;
  cube.name = "Cube";
  cube.mesh = 0;
  desc.nodes = {test::camera_at(kCamera), cube};
  return desc;
}

void show_picture(SceneDesc& desc, int image) {
  desc.materials[0].base_color_image = image;
  desc.materials[0].emissive_image = image;
}

}  // namespace

TEST_CASE("a picture put on a material after it was uploaded is drawn, swapped and taken off") {
  RenderStage stage;
  SceneDesc desc = plain_cube();
  stage.upload(desc);
  const Picture before = stage.draw(Scene::from_desc(desc), "material_picture_none");
  CHECK(before.find(pink).count == 0);

  desc.images.push_back(picture("lossless.webp", ".webp"));
  show_picture(desc, 0);
  stage.sync(desc);
  const Picture given = stage.draw(Scene::from_desc(desc), "material_picture_given");
  CHECK(given.find(pink).count > kVisiblePixels);
  CHECK(given.find(green).count > kVisiblePixels);

  desc.images.push_back(picture("sniff.png", ".png"));  // all green
  show_picture(desc, 1);
  stage.sync(desc);
  const Picture swapped = stage.draw(Scene::from_desc(desc), "material_picture_swapped");
  CHECK(swapped.find(pink).count == 0);
  CHECK(swapped.find(green).count > kVisiblePixels);

  show_picture(desc, kNoIndex);
  stage.sync(desc);
  const Picture cleared = stage.draw(Scene::from_desc(desc), "material_picture_cleared");
  CHECK(cleared.find(pink).count == 0);
  CHECK(cleared.find(green).count == 0);
}
