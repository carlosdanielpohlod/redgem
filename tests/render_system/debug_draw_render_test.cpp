#include <doctest/doctest.h>
#include <algorithm>

#include "render/debug_draw.hpp"
#include "app/window_presenter.hpp"
#include "app/engine.hpp"
#include "core/config.hpp"
#include "test_support.hpp"
#include "render_system/render_stage.hpp"

using namespace redgem;
namespace {
constexpr Vec3 kCamera{0.0f, 0.0f, 5.0f};
constexpr Vec3 kWallSize{2.0f, 2.0f, 2.0f};
constexpr Vec3 kBehind{0.0f, 0.0f, -2.0f};
constexpr float kHalfLine = 0.5f;
constexpr unsigned char kBrightGreen = 200;
constexpr unsigned char kOtherChannel = 32;
constexpr redgem::Color kRed{1.0f, 0.0f, 0.0f, 1.0f};
constexpr redgem::Color kGreen{0.0f, 1.0f, 0.0f, 1.0f};
bool green(::Color pixel) { return pixel.g > kBrightGreen && pixel.r < kOtherChannel && pixel.b < kOtherChannel; }
bool yellow(::Color pixel) { return pixel.r > kBrightGreen && pixel.g > kBrightGreen && pixel.b < kOtherChannel; }
bool same_pixel(::Color a, ::Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
SceneDesc wall_scene() {
  SceneDesc desc;
  desc.materials = {test::glowing_material(kRed, 1.0f)};
  desc.meshes = {test::cube_mesh(0)};
  NodeDesc wall;
  wall.mesh = 0;
  wall.local.scale = kWallSize;
  NodeDesc environment;
  environment.props["environment"] = true;
  environment.props["antialias"] = std::string("fxaa");
  desc.nodes = {test::camera_at(kCamera), wall, environment};
  return desc;
}
}  // namespace
TEST_CASE("debug lines and boxes obey scene depth and explicitly support x-ray") {
  DebugDrawItem command;
  command.color = kGreen;
  std::string name;
  SUBCASE("line") {
    name = "line";
    command.first = kBehind - Vec3{kHalfLine, 0.0f, 0.0f};
    command.second = kBehind + Vec3{kHalfLine, 0.0f, 0.0f};
  }
  SUBCASE("box") {
    name = "box";
    command.kind = DebugDrawItem::Kind::Box;
    command.first = kBehind;
    command.second = {1.0f, 1.0f, 1.0f};
  }
  test::RenderStage stage;
  stage.set_clock(0.0);
  const auto desc = wall_scene();
  stage.upload(desc);
  const auto scene = Scene::from_desc(desc);
  DebugDrawBuffer commands;
  REQUIRE(commands.append(command));
  CHECK(stage.draw_debug(scene, commands, "debug_" + name + "_hidden").find(green).count == 0);
  commands.clear();
  command.depth_test = false;
  REQUIRE(commands.append(command));
  CHECK(stage.draw_debug(scene, commands, "debug_" + name + "_xray").find(green).count > 0);
  commands.clear();
  auto tested = command;
  tested.depth_test = true;
  auto xray = command;
  xray.color = {1.0f, 1.0f, 0.0f, 1.0f};
  constexpr float kSeparation = 0.25f;  // distinguish the two depth batches in pixel readback
  tested.first.y += kSeparation;
  xray.first.y -= kSeparation;
  if (command.kind == DebugDrawItem::Kind::Line) {
    tested.second.y += kSeparation;
    xray.second.y -= kSeparation;
  }
  REQUIRE(commands.append(tested));
  REQUIRE(commands.append(xray));
  const auto mixed = stage.draw_debug(scene, commands, "debug_" + name + "_mixed");
  CHECK(mixed.find(green).count == 0);
  CHECK(mixed.find(yellow).count > 0);
  commands.clear();
  command.depth_test = true;
  command.first.z = 2.0f;
  if (command.kind == DebugDrawItem::Kind::Line) command.second.z = 2.0f;
  REQUIRE(commands.append(command));
  CHECK(stage.draw_debug(scene, commands, "debug_" + name + "_front").find(green).count > 0);
  commands.clear();
  constexpr int kReflectionWarmup = 12;  // two full cycles of the progressive six-face panorama
  for (int frame = 0; frame < kReflectionWarmup; ++frame) stage.draw(scene, "debug_reflection_warmup");
  const auto clean = stage.draw(scene, "debug_" + name + "_clean");
  const auto empty = stage.draw_debug(scene, commands, "debug_" + name + "_empty");
  CHECK(clean.pixels.size() == empty.pixels.size());
  CHECK(std::equal(clean.pixels.begin(), clean.pixels.end(), empty.pixels.begin(), same_pixel));
}

TEST_CASE("frame views draw depth-tested and x-ray shapes in native and scaled windows") {
  test::Picture latest;
  std::string capture;
  GameWindowOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.capture_cursor = false;
  DisplaySettings display;
  display.resolution = {256, 256};
  display.quality = QualityPreset::High;
  display.vsync = false;
  display.fps_cap = 0;
  SUBCASE("native window target") {}
  SUBCASE("half-resolution world target") { display.render_scale = 0.5f; }
  options.display = display;
  const std::string mode = display.render_scale < 1.0f ? "scaled" : "native";
  const auto folder = std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "debug-window" / mode;
  std::filesystem::create_directories(folder);
  options.on_frame_drawn = [&] {
    Image image = LoadImageFromScreen();
    REQUIRE(image.data != nullptr);
    const auto* pixels = static_cast<const ::Color*>(image.data);
    latest = {image.width, image.height, {pixels, pixels + image.width * image.height}};
    if (!capture.empty()) ExportImage(image, (folder / capture).string().c_str());
    UnloadImage(image);
  };
  WindowPresenter window(options);
  auto desc = wall_scene();
  desc.nodes.back().props["antialias"] = std::string("temporal");
  desc.nodes[1].name = "Wall";
  EngineOptions run;
  run.game_dir = options.game_dir;
  run.scene = &desc;
  run.presenter = &window;
  run.fixed_step = true;
  run.display = display;
  auto made = Engine::create(run);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(made));
  auto& engine = *std::get<std::unique_ptr<Engine>>(made);
  const auto ruby = [&](const char* code) {
    const auto result = engine.scripts().eval(code, "draw_window.rb");
    REQUIRE_MESSAGE(result.ok, result.error);
  };
  ruby(R"(
    class WindowWire < Behaviour
      def update(_dt)
        return if $hide_wire
        z = $front_wire ? 2 : -2
        Debug.line(Vector3.new(-0.5,0,z), Vector3.new(0.5,0,z), color: [0,255,0], depth_test: !$xray_wire)
      end
    end
    Scene.find('Wall').add_component(WindowWire)
  )");
  const auto visible = [](::Color pixel) {
    constexpr int kMinimumGreen = 120;
    constexpr int kChannelContrast = 2;
    return pixel.g > kMinimumGreen && pixel.g > pixel.r * kChannelContrast && pixel.g > pixel.b * kChannelContrast;
  };
  capture = "hidden.png";
  engine.frame(config::kFixedDeltaSeconds);
  CHECK(latest.find(visible).count == 0);
  ruby("$xray_wire = true");
  capture = "xray.png";
  engine.frame(config::kFixedDeltaSeconds);
  CHECK(latest.find(visible).count > 0);
  ruby("$xray_wire = false; $front_wire = true");
  capture = "front.png";
  engine.frame(config::kFixedDeltaSeconds);
  CHECK(latest.find(visible).count > 0);
  ruby("$hide_wire = true");
  capture = "cleared.png";
  engine.frame(config::kFixedDeltaSeconds);
  CHECK(latest.find(visible).count == 0);
  display.resolution = {320, 240};
  window.apply_display(display);
  ruby("$hide_wire = false");
  capture = "resized.png";
  constexpr int kResizeFrames = 3;
  for (int frame = 0; frame < kResizeFrames; ++frame) engine.frame(config::kFixedDeltaSeconds);
  CHECK(latest.find(visible).count > 0);
}
