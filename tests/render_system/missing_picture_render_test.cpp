// Missing images remain visible on meshes and HUDs; valid images and untextured surfaces keep their appearance.
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "core/log.hpp"
#include "render/texture_loading.hpp"
#include "render_system/render_stage.hpp"

using namespace redgem;
using namespace redgem::render_detail;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr ::Color kValidColor{0, 255, 0, 255};
constexpr int kImageSide = 64;
constexpr int kNextImage = 96;  // pixels between the two HUD images
constexpr float kMiddle = 0.5f;
constexpr float kValidAcross = 0.5f;
constexpr float kHudDown = 0.125f;
constexpr unsigned char kBright = 150;
constexpr unsigned char kDark = 70;
constexpr std::size_t kVisiblePixels = 100;
constexpr Vec3 kCamera{0, 0, 3};
constexpr const char* kMissingName = "not_here";

bool magenta(::Color c) { return c.r > kBright && c.b > kBright && c.g < kDark; }
bool black(::Color c) { return c.r < kDark && c.g < kDark && c.b < kDark; }
bool green(::Color c) { return c.g > kBright && c.r < kDark && c.b < kDark; }

class PictureFiles {
 public:
  const std::filesystem::path folder = std::filesystem::temp_directory_path() / "redgem-missing-picture-render";
  PictureFiles() {
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    Image image = GenImageColor(kImageSide, kImageSide, kValidColor);
    REQUIRE(ExportImage(image, (folder / "valid.png").string().c_str()));
    UnloadImage(image);
    std::ofstream(folder / "broken.png", std::ios::binary) << "broken png";
  }
  ~PictureFiles() { std::filesystem::remove_all(folder); }
};

class PictureWarnings {
 public:
  PictureWarnings() {
    previous_ = log::set_sink([this](log::Level, const std::string& message) { messages_.push_back(message); });
  }
  ~PictureWarnings() { log::set_sink(std::move(previous_)); }
  int containing(const std::string& text) const {
    return static_cast<int>(std::count_if(messages_.begin(), messages_.end(), [&](const std::string& message) {
      return message.find(text) != std::string::npos;
    }));
  }

 private:
  log::Sink previous_;
  std::vector<std::string> messages_;
};

HudItem image_at(const std::string& name, int x) {
  HudItem item;
  item.kind = HudItem::Kind::Image;
  item.text = name;
  item.x = x;
  item.width = kImageSide;
  item.height = kImageSide;
  return item;
}

void check_opaque_checker(Texture2D texture) {
  REQUIRE(texture.id != 0);
  Image decoded = LoadImageFromTexture(texture);
  ::Color* pixels = LoadImageColors(decoded);
  REQUIRE(pixels != nullptr);
  const auto count = static_cast<std::size_t>(decoded.width) * static_cast<std::size_t>(decoded.height);
  CHECK(std::count_if(pixels, pixels + count, magenta) > kVisiblePixels);
  CHECK(std::count_if(pixels, pixels + count, black) > kVisiblePixels);
  CHECK(std::all_of(pixels, pixels + count, [](const ::Color& c) { return c.a == 255; }));
  UnloadImageColors(pixels);
  UnloadImage(decoded);
  UnloadTexture(texture);
}

SceneDesc image_scene() {
  SceneDesc desc;
  desc.images = {ImageData{}};
  MaterialDesc material;
  material.base_color_image = 0;
  material.emissive_image = 0;
  material.emissive = {1, 1, 1};  // see the checker without relying on scene lighting
  desc.materials = {material};
  desc.meshes = {test::cube_mesh(0)};
  NodeDesc cube;
  cube.name = "Cube";
  cube.mesh = 0;
  desc.nodes = {test::camera_at(kCamera), cube};
  return desc;
}

}  // namespace

TEST_CASE("missing and unreadable named pictures are opaque independently owned checkerboards") {
  RenderStage stage;
  PictureFiles files;
  PictureWarnings warnings;
  check_opaque_checker(load_picture_file(files.folder, kMissingName));
  check_opaque_checker(load_picture_file(files.folder, "broken"));
  check_opaque_checker(load_picture_file(files.folder, "../valid"));
  CHECK(warnings.containing("using a checkerboard") == 3);
  CHECK(warnings.containing("is refused") == 1);
}

TEST_CASE("HUD missing picture is visible and cached while a valid picture retains its pixels") {
  RenderStage stage;
  PictureFiles files;
  stage.set_images_dir(files.folder);
  PictureWarnings warnings;
  const std::vector<HudItem> hud{image_at(kMissingName, 0), image_at("valid", kNextImage)};
  const Picture picture = stage.draw_hud(hud, "missing_picture_hud");
  CHECK(picture.find(magenta).count > kVisiblePixels);
  CHECK(picture.find(black).count > kVisiblePixels);
  CHECK(green(picture.at(kValidAcross, kHudDown)));
  stage.draw_hud(hud, "missing_picture_hud_cached");
  CHECK(warnings.containing("using a checkerboard") == 1);
  stage.set_images_dir(files.folder / "changed");
  stage.draw_hud({image_at(kMissingName, 0)}, "missing_picture_new_folder");
  CHECK(warnings.containing("using a checkerboard") == 2);
}

TEST_CASE("missing scene color texture is visible and an untextured material keeps its plain color") {
  RenderStage stage;
  SceneDesc desc = image_scene();
  SUBCASE("missing data") {}
  SUBCASE("undecodable data") { desc.images[0].encoded = {1, 2, 3}; }
  SUBCASE("no texture was specified") {
    desc.materials[0].base_color_image = kNoIndex;
    desc.materials[0].emissive_image = kNoIndex;
    desc.materials[0].base_color = {0, 1, 0, 1};
    desc.materials[0].emissive = {0, 1, 0};
  }
  stage.upload(desc);
  const Picture picture = stage.draw(Scene::from_desc(desc), "missing_picture_scene");
  if (desc.materials[0].base_color_image == kNoIndex) {
    CHECK(picture.find(magenta).count == 0);
    CHECK(green(picture.at(kMiddle, kMiddle)));
  } else {
    CHECK(picture.find(magenta).count > kVisiblePixels);
  }
}
