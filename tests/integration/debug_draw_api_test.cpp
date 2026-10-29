#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "render/debug_draw.hpp"
#include "test_support.hpp"

using namespace redgem;
namespace {
class DrawPresenter final : public Presenter {
 public:
  SurfaceSize surface_size() const override { return {640, 360}; }
  int text_width(const std::string&, int) const override { return 0; }
  void capture_cursor(bool) override {}
  bool open() const override { return true; }
  float frame_seconds() const override { return config::kFixedDeltaSeconds; }
  void present(const FrameView& frame) override {
    REQUIRE(frame.debug_draw != nullptr);
    seen = frame.debug_draw->items();
  }
  std::vector<DebugDrawItem> seen;
};
struct DrawGame {
  DrawPresenter presenter;
  std::unique_ptr<Engine> engine;
  explicit DrawGame(bool visible = true) {
    EngineOptions options;
    options.game_dir = test::fixture("games/sandbox");
    options.fixed_step = true;
    if (visible) options.presenter = &presenter;
    auto made = Engine::create(options);
    REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(made));
    engine = std::move(std::get<std::unique_ptr<Engine>>(made));
  }
  std::string ruby(const std::string& code) {
    const auto result = engine->scripts().eval(code, "draw_test.rb");
    REQUIRE_MESSAGE(result.ok, result.error);
    return RSTRING_PTR(mrb_obj_as_string(engine->scripts().vm().state(), result.value));
  }
  void frame() { engine->frame(config::kFixedDeltaSeconds); }
};
constexpr const char* kPainter = R"(
  class WirePainter < Behaviour
    def update(_dt)
      return if @drawn
      @drawn = true
      Debug.line(Vector3.new(-1, 2, 3), Vector3.new(1, 2, 3), color: :red, alpha: 128)
      Debug.box(Vector3.new(4, 5, 6), Vector3.new(1, 2, 3), color: [0, 255, 0], depth_test: false)
    end
  end
  Scene.find('Hero').add_component(WirePainter)
)";
}  // namespace
TEST_CASE("debug primitives reach the presenter in world coordinates for one frame only") {
  DrawGame game;
  game.ruby(kPainter);
  game.frame();
  const auto& seen = game.presenter.seen;
  REQUIRE(seen.size() == 2);
  CHECK(seen[0].kind == DebugDrawItem::Kind::Line);
  CHECK(seen[0].first.x == -1);
  CHECK(seen[0].second.x == 1);
  CHECK(seen[0].color.a == doctest::Approx(128.0 / 255.0));
  CHECK(seen[0].depth_test);
  CHECK(seen[1].kind == DebugDrawItem::Kind::Box);
  CHECK(seen[1].first.y == 5);
  CHECK(seen[1].second.z == 3);
  CHECK(seen[1].color.g == 1);
  CHECK_FALSE(seen[1].depth_test);
  game.frame();
  CHECK(game.presenter.seen.empty());
  game.ruby("Game.load_scene('level2')");
  game.frame();
  game.frame();
  CHECK(game.presenter.seen.empty());
}
TEST_CASE("debug commands reject invalid geometry and color without allocating on a dedicated server") {
  DrawGame game(false);
  CHECK(game.engine->script_context().debug_draw == nullptr);
  CHECK(game.ruby("Debug.line(Vector3.zero, Vector3.up)") == "false");
  CHECK(game.ruby("Debug.box(Vector3.zero, Vector3.new(1,1,1))") == "false");
  CHECK_FALSE(game.engine->scripts().eval("Debug.box(Vector3.zero, Vector3.new(-1,1,1))", "invalid_draw.rb").ok);
  CHECK_FALSE(
      game.engine->scripts().eval("Debug.line(Vector3.zero, Vector3.up, color: [256,0,0])", "invalid_draw.rb").ok);
  CHECK_FALSE(game.engine->scripts().eval("Debug.line(Vector3.zero, Vector3.up, alpha: -1)", "invalid_draw.rb").ok);
}
TEST_CASE("debug command capacity is bounded through Ruby and is renewed every frame") {
  DrawGame game;
  const auto limit = std::to_string(kMaxDebugDrawCommands);
  CHECK(game.ruby(limit + ".times.all? { Debug.line(Vector3.zero, Vector3.up) }") == "true");
  CHECK(game.ruby("Debug.box(Vector3.zero, Vector3.up)") == "false");
  REQUIRE(game.engine->script_context().debug_draw != nullptr);
  CHECK(game.engine->script_context().debug_draw->dropped() == 1);
  game.frame();
  CHECK(game.engine->script_context().debug_draw->items().empty());
  CHECK(game.engine->script_context().debug_draw->dropped() == 0);
  CHECK(game.ruby("Debug.line(Vector3.zero, Vector3.up)") == "true");
}
