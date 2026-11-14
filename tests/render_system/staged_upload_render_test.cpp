#include <doctest/doctest.h>
#include <raylib.h>

#include <filesystem>
#include <vector>

#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "render/texture_loading.hpp"
#include "render_system/render_stage.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::RenderStage;

namespace {
constexpr std::size_t kOneByte = 1;  // less than any asset: one asset a call
constexpr std::size_t kEverything = static_cast<std::size_t>(-1);
constexpr int kStagedMeshes = 2;
constexpr int kStagedScene = 1;
constexpr int kOtherScene = 2;

ImageData picture(const char* name, const char* extension) {
  const auto bytes = read_file_up_to(std::filesystem::path(test::fixture("images")) / name, kMaxEncodedImageBytes);
  REQUIRE(bytes.has_value());
  return ImageData{*bytes, extension};
}

SceneDesc two_pictures_two_meshes() {
  SceneDesc desc;
  desc.images = {picture("lossless.webp", ".webp"), picture("sniff.png", ".png")};
  desc.decoded_images = {render_detail::decode_scene_picture(desc.images[0]), DecodedPicture{}};
  MaterialDesc material;
  material.base_color_image = 0;
  desc.materials = {material};
  for (int i = 0; i < kStagedMeshes; ++i) desc.meshes.push_back(test::cube_mesh(0));
  return desc;
}

int calls_until_staged(RenderStage& stage, const SceneDesc& desc, std::size_t bytes) {
  int calls = 1;
  while (!stage.stage(desc, kStagedScene, bytes)) ++calls;
  return calls;
}
}  // namespace

TEST_CASE("a scene's assets are staged a bounded share a call, then taken by its upload") {
  RenderStage stage;
  const SceneDesc desc = two_pictures_two_meshes();
  REQUIRE_FALSE(desc.decoded_images[0].empty());
  const auto assets = static_cast<int>(desc.images.size() + desc.meshes.size());
  CHECK(calls_until_staged(stage, desc, kOneByte) == assets);
  stage.upload(desc, kStagedScene);
  const Texture2D decoded = stage.scene_texture(0);
  CHECK(decoded.width == desc.decoded_images[0].width);
  CHECK(decoded.height == desc.decoded_images[0].height);
  CHECK(stage.scene_texture(1).id != 0);
}

TEST_CASE("assets staged for one scene are never taken by the upload of another") {
  RenderStage stage;
  const SceneDesc staged = two_pictures_two_meshes();
  CHECK(calls_until_staged(stage, staged, kEverything) == 1);
  SceneDesc other;
  other.images = {picture("sniff.png", ".png")};
  stage.upload(other, kOtherScene);
  CHECK(stage.scene_texture(0).id != 0);
  CHECK(stage.scene_texture(1).id == 0);
}

TEST_CASE("staging another scene starts over") {
  RenderStage stage;
  const SceneDesc first = two_pictures_two_meshes();
  CHECK_FALSE(stage.stage(first, kStagedScene, kOneByte));
  CHECK_FALSE(stage.stage(first, kOtherScene, kOneByte));  // (one asset again, of four)
  CHECK(calls_until_staged(stage, first, kOneByte) == static_cast<int>(first.images.size() + first.meshes.size()));
}

TEST_CASE("a staged KTX2 picture goes up as it would unstaged, compressed where the card can") {
  RenderStage stage;
  SceneDesc desc;
  desc.images = {picture("etc1s_opaque.ktx2", ".ktx2")};
  desc.decoded_images = {render_detail::decode_scene_picture(desc.images[0])};
  CHECK(calls_until_staged(stage, desc, kEverything) == 1);
  stage.upload(desc, kStagedScene);
  const Texture2D unstaged = render_detail::load_scene_texture(desc.images[0]);
  CHECK(stage.scene_texture(0).format == unstaged.format);
  CHECK(stage.scene_texture(0).mipmaps == unstaged.mipmaps);
  UnloadTexture(unstaged);
}
