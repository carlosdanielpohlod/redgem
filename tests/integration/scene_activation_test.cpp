#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>

#include "app/engine.hpp"
#include "app/scene_activation.hpp"
#include "app/scene_loader.hpp"
#include "core/config.hpp"
#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "render/texture_loading.hpp"
#include "scene/props.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {
constexpr int kCrates = 600;          // far more bodies than one frame's share makes
constexpr double kWorkTick = 0.0005;  // each reading of the work clock: one piece of work done
constexpr int kFrameLimit = 100000;   // a load that never finishes fails instead of hanging
constexpr float kCrateSpacing = 2.0f;
constexpr const char* kLevel = "level2";  // (its scene script is the sandbox's)

// A clock that only moves when it is read: the time activation takes is how often it asks.
struct WorkClock {
  double now = 0.0;
  int reads = 0;
  Stopwatch::Reading reading() {
    return [this] {
      ++reads;
      return now += kWorkTick;
    };
  }
};

NodeDesc crate(int number) {
  NodeDesc node;
  node.name = "Crate" + std::to_string(number);
  node.local.position = {kCrateSpacing * static_cast<float>(number), 0.0f, 0.0f};
  node.props[props::kBody] = std::string(props::kBodyStatic);
  node.props[props::kTag] = std::string("crate");
  return node;
}

// A big level: a counter of its crates, and the crates, each with a body.
LoadResult generated_level() {
  LoadResult result;
  result.ok = true;
  NodeDesc counter;
  counter.name = "Counter";
  counter.props[props::kScript] = std::string("crate_counter.rb");
  result.scene.nodes.push_back(counter);
  for (int i = 0; i < kCrates; ++i) result.scene.nodes.push_back(crate(i));
  return result;
}

std::unique_ptr<Engine> game(SceneLoader& loader, WorkClock& clock, bool fixed) {
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.scene_loader = &loader;
  options.activation_time = clock.reading();
  options.fixed_step = fixed;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

std::string ruby(Engine& engine, const std::string& code) {
  const auto result = engine.scripts().eval(code, "scene_activation_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

void begin_loading(Engine& engine) {
  ruby(engine, std::string("$loading_draws = 0; Game.on_loading { $loading_draws += 1 }; Game.load_scene_async('") +
                   kLevel + "')");
}

bool every_crate_has_a_body(Engine& engine) {
  const auto crates = engine.scene().find_all_by_tag("crate");
  for (const ObjectId id : crates) {
    if (!engine.physics().has_object(id)) return false;
  }
  return crates.size() == static_cast<std::size_t>(kCrates);
}
}  // namespace

TEST_CASE("a big level is made ready over many frames, none taking more than its share") {
  SceneLoader loader([](const auto&, const auto&) { return generated_level(); });
  WorkClock clock;
  auto engine = game(loader, clock, false);
  begin_loading(*engine);
  int loading_frames = 0;
  double longest = 0.0;
  while (engine->scene_name() != kLevel && loading_frames < kFrameLimit) {
    const int before = clock.reads;
    engine->frame(config::kFixedDeltaSeconds);
    ++loading_frames;
    // (The first reading starts the frame's share; each after it ends a piece of work.)
    const int pieces = std::max(clock.reads - before - 1, 0);
    longest = std::max(longest, pieces * kWorkTick);
  }
  REQUIRE(engine->scene_name() == kLevel);
  CHECK(loading_frames > 2);
  CHECK(longest <= kActivationSecondsPerFrame + kWorkTick);                      // the margin: the piece that ran over
  CHECK(ruby(*engine, "$loading_draws") == std::to_string(loading_frames - 1));  // all but the swap's
  CHECK(every_crate_has_a_body(*engine));
}

TEST_CASE("scripts of a level made ready over frames see all of its objects on their first update") {
  SceneLoader loader([](const auto&, const auto&) { return generated_level(); });
  WorkClock clock;
  auto engine = game(loader, clock, false);
  begin_loading(*engine);
  for (int i = 0; i < kFrameLimit && engine->scene_name() != kLevel; ++i) engine->frame(config::kFixedDeltaSeconds);
  REQUIRE(engine->scene_name() == kLevel);
  engine->frame(config::kFixedDeltaSeconds);
  CHECK(ruby(*engine, "$crates_seen") == std::to_string(kCrates));
  CHECK(ruby(*engine, "$level2_setup_in") == kLevel);
}

TEST_CASE("offline fixed-step runs make a big level ready in the frame its read ends, at once") {
  SceneLoader loader([](const auto&, const auto&) { return generated_level(); });
  WorkClock clock;
  auto engine = game(loader, clock, true);
  begin_loading(*engine);
  engine->frame(config::kFixedDeltaSeconds);
  CHECK(engine->scene_name() == "main");
  engine->frame(config::kFixedDeltaSeconds);
  CHECK(engine->scene_name() == kLevel);
  CHECK(ruby(*engine, "$loading_draws") == "1");
  CHECK(clock.reads == 0);  // no frame budget: the real time cannot change what happens
  CHECK(every_crate_has_a_body(*engine));
}

TEST_CASE("cancelling a level while it is made ready keeps the running scene") {
  SceneLoader loader([](const auto&, const auto&) { return generated_level(); });
  WorkClock clock;
  auto engine = game(loader, clock, false);
  begin_loading(*engine);
  for (int i = 0; i < kFrameLimit && clock.reads == 0; ++i) engine->frame(config::kFixedDeltaSeconds);
  REQUIRE(clock.reads > 0);  // the level is being made ready
  CHECK(ruby(*engine, "Game.cancel_scene_load") == "true");
  engine->frame(config::kFixedDeltaSeconds);
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "Game.loading?") == "false");
  CHECK(engine->scene().find("Hero") != kNoObject);
}

TEST_CASE("a scene read for a game that draws it has its pictures decoded in the background") {
  const auto bytes =
      read_file_up_to(std::filesystem::path(test::fixture("images")) / "sniff.png", kMaxEncodedImageBytes);
  REQUIRE(bytes.has_value());
  SceneLoader loader([&](const auto&, const auto&) {
    LoadResult result;
    result.ok = true;
    result.scene.images = {ImageData{*bytes, ".png"}};
    return result;
  });
  auto drawn = loader.submit(test::fixture("games/sandbox"), kLevel, true);
  auto undrawn = loader.submit(test::fixture("games/sandbox"), kLevel, false);
  drawn->wait_until_ready();
  undrawn->wait_until_ready();
  const auto decoded = drawn->take()->loaded.scene.decoded_images;
  REQUIRE(decoded.size() == 1);
  CHECK(decoded[0].width > 0);
  CHECK_FALSE(decoded[0].empty());
  CHECK_FALSE(decoded[0].unreadable);
  CHECK(undrawn->take()->loaded.scene.decoded_images.empty());
}

TEST_CASE("a picture that cannot be decoded in the background is marked so, and not tried again") {
  const DecodedPicture broken = render_detail::decode_scene_picture(ImageData{{1, 2, 3}, ".png"});
  CHECK(broken.unreadable);
  CHECK(broken.empty());
}
