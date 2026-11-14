#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "core/gpu_texture_formats.hpp"
#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "render/compressed_textures.hpp"
#include "render/texture_loading.hpp"
#include "render_system/render_stage.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::RenderStage;

namespace {

// tests/fixtures/images/*.ktx2 (see tools/assets/make_ktx2_fixtures.py).
constexpr int kOpaqueWidth = 16;  // ETC1S, red left half, blue right half
constexpr int kOpaqueHeight = 8;
constexpr int kOpaqueLevels = 5;
constexpr int kAlphaLevels = 4;  // UASTC, 8 x 8, see-through bottom half
constexpr std::size_t kVisiblePixels = 100;
constexpr unsigned char kBright = 150;
constexpr unsigned char kDark = 70;
constexpr Vec3 kCamera{0, 0, 3};

std::vector<std::uint8_t> ktx2_bytes(const std::string& name) {
  const auto bytes = read_file_up_to(test::fixture("images/" + name + ".ktx2"), kMaxEncodedImageBytes);
  REQUIRE(bytes.has_value());
  return *bytes;
}

bool red(::Color c) { return c.r > kBright && c.g < kDark && c.b < kDark; }
bool blue(::Color c) { return c.b > kBright && c.r < kDark && c.g < kDark; }

// Whether this card takes one of the compressed formats (a picture of either kind then stays so).
bool card_compresses() { return is_block_compressed(best_block_format(render_detail::gpu_texture_support(), false)); }

bool compressed(const Texture2D& texture) { return texture.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB; }

SceneDesc textured_cube() {
  SceneDesc desc;
  desc.images = {ImageData{ktx2_bytes("etc1s_opaque"), ".ktx2"}};
  MaterialDesc material;
  material.base_color_image = 0;
  material.emissive_image = 0;
  material.emissive = {1, 1, 1};
  desc.materials = {material};
  desc.meshes = {test::cube_mesh(0)};
  NodeDesc cube;
  cube.name = "Cube";
  cube.mesh = 0;
  desc.nodes = {test::camera_at(kCamera), cube};
  return desc;
}

}  // namespace

TEST_CASE("KTX2 scene textures stay compressed with their own mipmaps where the card takes them") {
  RenderStage stage;
  const GpuTextureSupport& support = render_detail::gpu_texture_support();
  MESSAGE("compressed formats: BCn " << support.bc << ", ASTC " << support.astc << ", ETC2 " << support.etc2);
  const Texture2D opaque = render_detail::load_scene_texture(ImageData{ktx2_bytes("etc1s_opaque"), ".ktx2"});
  const Texture2D clear = render_detail::load_scene_texture(ImageData{ktx2_bytes("uastc_alpha"), ".ktx2"});
  REQUIRE(opaque.id != 0);
  REQUIRE(clear.id != 0);
  CHECK(opaque.width == kOpaqueWidth);
  CHECK(opaque.height == kOpaqueHeight);
  CHECK(compressed(opaque) == card_compresses());
  CHECK(compressed(clear) == card_compresses());
  if (card_compresses()) {
    CHECK(opaque.mipmaps == kOpaqueLevels);
    CHECK(clear.mipmaps == kAlphaLevels);
  }
  UnloadTexture(opaque);
  UnloadTexture(clear);
}

TEST_CASE("a KTX2 base color and emission draw their source colors") {
  RenderStage stage;
  const auto desc = textured_cube();
  stage.upload(desc);
  const auto picture = stage.draw(Scene::from_desc(desc), "ktx2_scene_material");
  CHECK(picture.find(red).count > kVisiblePixels);
  CHECK(picture.find(blue).count > kVisiblePixels);
}
