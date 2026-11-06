#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <filesystem>
#include "core/config.hpp"
#include "core/files.hpp"
#include "editor/editor_session.hpp"
#include "scene/props.hpp"
#include "scene/scene_ids.hpp"
#include "script/ruby_vm.hpp"
#include "test_support.hpp"

using namespace redgem;
namespace {
void checked(RubyVm& vm, const std::string& source) {
  const auto result = vm.eval(source, "reload_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
}
void checked(ScriptHost& host, const std::string& source) { checked(host.vm(), source); }
constexpr const char* kOriginal = R"(
class Mover < Behaviour
  STEP = 1
  attr_reader :steps
  def start
    $actor = self
    @steps = 0
    $starts = ($starts || 0) + 1
  end
  def update(_dt)
    @steps += STEP
  end
  def obsolete
    42
  end
end
)";
constexpr const char* kChanged = R"(
class Mover < Behaviour
  STEP = 5
  attr_reader :steps
  def update(_dt)
    @steps += STEP
  end
  def fixed_update(_dt)
    $fixed = ($fixed || 0) + 1
  end
end
)";
struct ReloadGame {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / ("redgem-reload-" + make_scene_id());
  ReloadGame() {
    std::filesystem::create_directories(dir / "scripts");
    write(kOriginal);
    auto doc = SceneDocument::create_empty(dir / config::kSceneFileName);
    const int root = doc.add_primitive(PrimitiveShape::Cube, "Actor", Color{1, 0, 0, 1});
    doc.set_prop(root, props::kBody, std::string(props::kBodyDynamic));
    doc.set_prop(root, props::kScript, std::string("mover.rb"));
    std::string error;
    REQUIRE_MESSAGE(doc.save(error), error);
  }
  ~ReloadGame() {
    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
  }
  void write(const std::string& code) const {
    std::string error;
    REQUIRE_MESSAGE(write_text_file(dir / "scripts/mover.rb", code, error), error);
  }
  std::unique_ptr<EditorSession> open() const {
    auto opened = EditorSession::open(dir);
    if (auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
    return std::move(std::get<std::unique_ptr<EditorSession>>(opened));
  }
};
void start(EditorSession& session, ScriptedInputSource& input) {
  std::string error;
  REQUIRE_MESSAGE(session.play({input}, error), error);
  for (int i = 0; i < 3; ++i) session.tick(config::kFixedDeltaSeconds);
}
}  // namespace
TEST_CASE(
    "script reload: live class replacement preserves identity fields inheritance and GC roots while removing stale "
    "declarations") {
  RubyVm vm;
  checked(vm,
          "class Behaviour; end; class Mover < Behaviour; OLD = 8; STEP = 1; def steps; @n ||= 4; end; def obsolete; "
          "end; end; class Child < Mover; end; $actor = Child.new; $id = $actor.object_id; $klass = Mover; raise "
          "unless $actor.steps == 4");
  auto* klass = mrb_class_get(vm.state(), "Mover");
  for (int i = 0; i < 10; ++i) {
    const auto result =
        vm.reload_class(klass,
                        "class Mover < Behaviour; STEP = 2; VALUES = [1, -2, {key: 'value'}]; attr_reader :n; def "
                        "steps; @n += change; end; private; def change; STEP; end; end",
                        "mover.rb");
    REQUIRE_MESSAGE(result.ok, result.error);
    checked(vm,
            "raise unless $actor.object_id == $id && $klass == Mover; raise if $actor.respond_to?(:obsolete); raise "
            "unless $actor.steps == " +
                std::to_string(6 + i * 2));
    CHECK_FALSE(mrb_const_defined_at(vm.state(), mrb_obj_value(klass), mrb_intern_lit(vm.state(), "OLD")));
    mrb_full_gc(vm.state());
  }
}
TEST_CASE(
    "script reload: live class replacement rejects executable setup and declaration hooks before touching running "
    "code") {
  RubyVm vm;
  checked(vm, "class Behaviour; end; class Mover < Behaviour; def value; 7; end; end; $actor = Mover.new");
  auto* klass = mrb_class_get(vm.state(), "Mover");
  for (const auto* code :
       {"$changed = true; class Mover < Behaviour; end", "class Mover < Behaviour; X = Scene.clear; end",
        "class Other < Behaviour; end", "class Mover < Behaviour; X = -[1]; end",
        "class Mover < Behaviour; X = {[] => 1}; end", "class Mover < Behaviour; include Enumerable; end",
        "class Mover < Behaviour; def self.value; 9; end; end", "class Mover < Behaviour; def update("}) {
    CAPTURE(code);
    CHECK_FALSE(vm.reload_class(klass, code, "mover.rb").ok);
    checked(vm, "raise unless $actor.value == 7; raise if $changed");
  }
  checked(vm, "class Class; def attr_reader(*args); $changed = true; end; end");
  CHECK_FALSE(vm.reload_class(klass, "class Mover < Behaviour; attr_reader :count; end", "mover.rb").ok);
  checked(vm, "raise unless $actor.value == 7; raise if $changed");
  RubyVm mixed;
  checked(mixed,
          "class Behaviour; end; module Extra; def factory; 9; end; end; class Mover < Behaviour; extend Extra; def "
          "value; 7; end; end; $actor = Mover.new");
  auto* extended = mrb_class_get(mixed.state(), "Mover");
  CHECK_FALSE(mixed.reload_class(extended, "class Mover < Behaviour; def value; 9; end; end", "mover.rb").ok);
  checked(mixed, "raise unless $actor.value == 7");
}
TEST_CASE(
    "script reload: editor reload preserves behaviour state and physics without repeating start and refreshes fixed "
    "callbacks") {
  test::LogCapture quiet;
  ReloadGame game;
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  auto& engine = *session->runtime();
  auto& host = engine.scripts();
  checked(host, "$old_steps = $actor.steps; $id = $actor.object_id; $klass = Mover");
  const int actor = engine.scene().find("Actor");
  const Vec3 velocity{3, 4, 5};
  engine.physics().set_velocity(actor, velocity);
  const auto before = engine.scene().world_transform(actor);
  game.write(kChanged);
  REQUIRE(host.reload_scripts() == 1);
  CHECK(approx_equal(engine.physics().velocity(actor), velocity, test::kTolerance));
  CHECK(approx_equal(engine.scene().world_transform(actor).position, before.position, test::kTolerance));
  checked(host,
          "raise unless $actor.object_id == $id && Mover == $klass && $actor.steps == $old_steps && $starts == 1; "
          "raise if $actor.respond_to?(:obsolete)");
  session->tick(config::kFixedDeltaSeconds);
  checked(host, "raise unless $actor.steps == $old_steps + 5 && $fixed > 0 && $starts == 1");
  CHECK(host.reload_scripts() == 0);
  session->stop();
}
TEST_CASE("script reload: failed changed scripts retain last code and report once per version with manual retry") {
  test::LogCapture quiet;
  ReloadGame game;
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  auto& host = session->runtime()->scripts();
  game.write("class Mover < Behaviour; def update(");
  const auto errors = host.error_count();
  CHECK(host.reload_scripts() == 0);
  CHECK(host.error_count() == errors + 1);
  CHECK(host.reload_scripts() == 0);
  CHECK(host.error_count() == errors + 1);
  CHECK(host.reload_scripts(true) == 0);
  CHECK(host.error_count() == errors + 2);
  checked(host, "$old_steps = $actor.steps");
  session->tick(config::kFixedDeltaSeconds);
  checked(host, "raise unless $actor.steps == $old_steps + 1");
  std::filesystem::remove(game.dir / "scripts/mover.rb");
  CHECK(host.reload_scripts() == 0);
  CHECK(host.error_count() == errors + 3);
  CHECK(host.reload_scripts() == 0);
  CHECK(host.error_count() == errors + 3);
  game.write(kChanged);
  CHECK(host.reload_scripts() == 1);
  std::filesystem::rename(game.dir / "scripts", game.dir / "original_scripts");
  std::filesystem::create_directory_symlink(game.dir / "original_scripts", game.dir / "scripts");
  CHECK(host.reload_scripts() == 0);
  CHECK(host.error_count() == errors + 4);
  CHECK(host.reload_scripts() == 0);
  CHECK(host.error_count() == errors + 4);
  session->stop();
}
TEST_CASE("script reload: ordinary game engines never watch or replace script sources") {
  test::LogCapture quiet;
  ReloadGame game;
  EngineOptions options;
  options.game_dir = game.dir;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  auto& engine = *std::get<std::unique_ptr<Engine>>(created);
  engine.frame(config::kFixedDeltaSeconds);
  game.write(kChanged);
  CHECK(engine.scripts().reload_scripts(true) == 0);
  checked(engine.scripts(), "raise unless Mover::STEP == 1");
}

TEST_CASE("script reload: failed installation restores method and constant tables") {
  RubyVm vm;
  checked(vm, "class Behaviour; end; class Mover < Behaviour; OLD = 4; def value; OLD; end; end; $actor = Mover.new");
  auto* klass = mrb_class_get(vm.state(), "Mover");
  const auto result = vm.reload_class(
      klass, "class Mover < Behaviour; NEW = 8; def value; NEW; end; private :missing; end", "mover.rb");
  CHECK_FALSE(result.ok);
  mrb_full_gc(vm.state());
  checked(vm, "raise unless $actor.value == 4 && Mover::OLD == 4");
  CHECK_FALSE(mrb_const_defined_at(vm.state(), mrb_obj_value(klass), mrb_intern_lit(vm.state(), "NEW")));
}

TEST_CASE("script reload: an orphaned class is not cleared when its name would define a different class") {
  RubyVm vm;
  checked(vm,
          "class Behaviour; end; class Mover < Behaviour; def value; 7; end; end; $actor = Mover.new; Alias = Mover; "
          "Object.send(:remove_const, :Mover)");
  auto* klass = mrb_class_get(vm.state(), "Alias");
  CHECK_FALSE(vm.reload_class(klass, "class Mover < Behaviour; def value; 9; end; end", "mover.rb").ok);
  checked(vm, "raise unless $actor.value == 7");
  checked(vm,
          "Object.const_set(:Mover, Alias); SavedBase = Behaviour; Object.send(:remove_const, :Behaviour); def "
          "Object.const_missing(name); $autoload = true; SavedBase; end");
  CHECK_FALSE(vm.reload_class(klass, "class Mover < Behaviour; def value; 9; end; end", "mover.rb").ok);
  checked(vm, "raise unless $actor.value == 7; raise if $autoload");
}

TEST_CASE("script reload: a corrected callback resumes its disabled instance without repeating start") {
  test::LogCapture quiet;
  ReloadGame game;
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  auto& host = session->runtime()->scripts();
  checked(host, "$before = $actor.steps");
  game.write("class Mover < Behaviour; def update(_dt); raise 'broken'; end; end");
  REQUIRE(host.reload_scripts() == 1);
  const int errors = host.error_count();
  session->tick(config::kFixedDeltaSeconds);
  CHECK(host.error_count() == errors + 1);
  game.write(kChanged);
  REQUIRE(host.reload_scripts() == 1);
  session->tick(config::kFixedDeltaSeconds);
  checked(host, "raise unless $actor.steps == $before + 5 && $starts == 1 && $fixed > 0");
  CHECK(host.error_count() == errors + 1);
  session->stop();
}
