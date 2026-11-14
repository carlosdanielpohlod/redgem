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
  while (!stage.stage(desc, bytes)) ++calls;
  return calls;
}
}  // namespace

TEST_CASE("a scene's assets are staged a bounded share a call, then taken by its upload") {
  RenderStage stage;
  const SceneDesc desc = two_pictures_two_meshes();
  REQUIRE_FALSE(desc.decoded_images[0].empty());
  const auto assets = static_cast<int>(desc.images.size() + desc.meshes.size());
  CHECK(calls_until_staged(stage, desc, kOneByte) == assets);
  stage.upload(desc);
  const Texture2D decoded = stage.scene_texture(0);
  CHECK(decoded.width == desc.decoded_images[0].width);
  CHECK(decoded.height == desc.decoded_images[0].height);
  CHECK(stage.scene_texture(1).id != 0);
}

TEST_CASE("assets staged for a scene that is not shown are dropped by the painter's next upload") {
  RenderStage stage;
  const SceneDesc staged = two_pictures_two_meshes();
  CHECK(calls_until_staged(stage, staged, kEverything) == 1);
  stage.drop_staged();
  SceneDesc other;
  other.images = {picture("sniff.png", ".png")};
  stage.upload(other);
  CHECK(stage.scene_texture(0).id != 0);
  CHECK(stage.scene_texture(1).id == 0);
}
