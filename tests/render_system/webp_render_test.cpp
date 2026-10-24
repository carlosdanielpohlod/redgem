#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <vector>

#include "core/safe_path.hpp"
#include "core/log.hpp"
#include "core/image_header.hpp"
#include "render/sky_picture.hpp"
#include "render/texture_loading.hpp"
#include "render_system/render_stage.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {
constexpr int kSide = 16;
constexpr std::size_t kVisiblePixels = 100;
constexpr unsigned char kBright = 150;
constexpr unsigned char kDark = 70;
constexpr unsigned char kHalfAlpha = 128;
constexpr int kBlendedWhite = 127;
constexpr float kLeftAcross = 0.125f;
constexpr float kRightAcross = 0.375f;
constexpr float kAcross = 0.25f;
constexpr int kHudSide = 128;
constexpr Vec3 kCamera{0, 0, 3};

std::filesystem::path images_folder() { return test::fixture("images"); }
std::vector<std::uint8_t> image_bytes() {
  const auto bytes = read_file_up_to(images_folder() / "lossless.webp", kMaxEncodedImageBytes);
  REQUIRE(bytes.has_value());
  return *bytes;
}
bool pink(::Color c) { return c.r > kBright && c.b > kBright && c.g < kDark; }
bool green(::Color c) { return c.g > kBright && c.r < kDark && c.b < kDark; }

SceneDesc textured_cube() {
  SceneDesc desc;
  desc.images = {ImageData{image_bytes(), ".webp"}};
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

TEST_CASE("WebP textures upload with their exact lossless color and alpha") {
  RenderStage stage;
  const Texture2D texture = render_detail::load_picture_file(images_folder(), "lossless");
  REQUIRE(texture.id != 0);
  REQUIRE(texture.width == kSide);
  Image image = LoadImageFromTexture(texture);
  ::Color* pixels = LoadImageColors(image);
  REQUIRE(pixels != nullptr);
  CHECK(pink(pixels[0]));
  CHECK(pixels[0].a == 255);
  CHECK(green(pixels[kSide - 1]));
  CHECK(pixels[kSide - 1].a == kHalfAlpha);
  UnloadImageColors(pixels);
  UnloadImage(image);
  UnloadTexture(texture);
}

TEST_CASE("HUD WebP transparency blends over its background") {
  RenderStage stage;
  stage.set_images_dir(images_folder());
  HudItem item;
  item.kind = HudItem::Kind::Image;
  item.text = "lossless";
  item.width = kHudSide;
  item.height = kHudSide;
  const Picture picture = stage.draw_hud({item}, "webp_hud_alpha");
  CHECK(pink(picture.at(kLeftAcross, kAcross)));
  const auto translucent = picture.at(kRightAcross, kAcross);
  CHECK(translucent.g == 255);
  CHECK(std::abs(static_cast<int>(translucent.r) - kBlendedWhite) <= 1);
  CHECK(std::abs(static_cast<int>(translucent.b) - kBlendedWhite) <= 1);
}

TEST_CASE("scene WebP base color and emission draw their source colors") {
  RenderStage stage;
  const auto desc = textured_cube();
  stage.upload(desc);
  const auto picture = stage.draw(Scene::from_desc(desc), "webp_scene_material");
  CHECK(picture.find(pink).count > kVisiblePixels);
  CHECK(picture.find(green).count > kVisiblePixels);
}

TEST_CASE("WebP sky pictures decode to linear light with the existing shrink limit") {
  constexpr std::uint32_t kShrunkSide = 8;
  const auto sky = render_detail::load_sky_picture(images_folder(), "lossless", kShrunkSide);
  REQUIRE(sky.has_value());
  CHECK(sky->width == kShrunkSide);
  CHECK(sky->height == kShrunkSide);
  CHECK(sky->at(0, 0).x == doctest::Approx(1));
  CHECK(sky->at(0, 0).y == doctest::Approx(0));
  CHECK(sky->at(kShrunkSide - 1, 0).y == doctest::Approx(1));
}

namespace {
struct DisguisedImages {
  const std::filesystem::path folder = std::filesystem::temp_directory_path() / "redgem-sniffed-images";
  DisguisedImages() { std::filesystem::create_directories(folder); }
  ~DisguisedImages() { std::filesystem::remove_all(folder); }
};
::Color first_texel(Texture2D texture) {
  REQUIRE(texture.id != 0);
  Image image = LoadImageFromTexture(texture);
  ::Color* pixels = LoadImageColors(image);
  REQUIRE(pixels != nullptr);
  const auto first = pixels[0];
  UnloadImageColors(pixels);
  UnloadImage(image);
  UnloadTexture(texture);
  return first;
}
struct SkyWarnings {
  int warnings = 0;
  log::Sink previous = log::set_sink([this](log::Level level, const std::string&) {
    if (level == log::Level::Warning) ++warnings;
  });
  ~SkyWarnings() { log::set_sink(std::move(previous)); }
};
}  // namespace

TEST_CASE("PNG and JPEG bytes use their sniffed decoder despite WebP filenames and glTF hints") {
  RenderStage stage;
  DisguisedImages files;
  for (const auto* extension : {"png", "jpg"}) {
    const auto source = images_folder() / (std::string("sniff.") + extension);
    const auto destination = files.folder / (std::string(extension) + ".webp");
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
    CHECK(green(first_texel(render_detail::load_picture_file(files.folder, destination.filename().string()))));
    const auto bytes = read_file_up_to(source, kMaxEncodedImageBytes);
    REQUIRE(bytes.has_value());
    CHECK(green(first_texel(render_detail::load_scene_texture(ImageData{*bytes, ".webp"}))));
    const auto sky = render_detail::load_sky_picture(files.folder, destination.filename().string(), kSide);
    REQUIRE(sky.has_value());
    CHECK(sky->at(0, 0).y > kHalf);
  }
}

TEST_CASE("an invalid or missing WebP sky reports one contextual warning") {
  const char* name = "animated";
  SUBCASE("animated") {}
  SUBCASE("missing") { name = "not-present"; }
  SkyWarnings captured;
  CHECK_FALSE(render_detail::load_sky_picture(images_folder(), name, kSide).has_value());
  CHECK(captured.warnings == 1);
}
