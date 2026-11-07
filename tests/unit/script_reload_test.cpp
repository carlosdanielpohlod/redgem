#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/class.h>
#include <mruby/proc.h>
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
std::size_t settled_reload(ScriptHost& host) {
  host.reload_scripts();
  return host.reload_scripts();
}
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
        "class Mover < Behaviour; X = {[] => 1}; end", "class Mover < Behaviour; X = %W[#{($changed = true)}]; end",
        "class Mover < Behaviour; include Enumerable; end", "class Mover < Behaviour; def self.value; 9; end; end",
        "class Mover < Behaviour; def update("}) {
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
  REQUIRE(settled_reload(host) == 1);
  CHECK(approx_equal(engine.physics().velocity(actor), velocity, test::kTolerance));
  CHECK(approx_equal(engine.scene().world_transform(actor).position, before.position, test::kTolerance));
  checked(host,
          "raise unless $actor.object_id == $id && Mover == $klass && $actor.steps == $old_steps && $starts == 1; "
          "raise if $actor.respond_to?(:obsolete)");
  session->tick(config::kFixedDeltaSeconds);
  checked(host, "raise unless $actor.steps == $old_steps + 5 && $fixed > 0 && $starts == 1");
  CHECK(settled_reload(host) == 0);
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
  CHECK(settled_reload(host) == 0);
  CHECK(host.error_count() == errors + 1);
  CHECK(settled_reload(host) == 0);
  CHECK(host.error_count() == errors + 1);
  CHECK(host.reload_scripts(true) == 0);
  CHECK(host.error_count() == errors + 2);
  checked(host, "$old_steps = $actor.steps");
  session->tick(config::kFixedDeltaSeconds);
  checked(host, "raise unless $actor.steps == $old_steps + 1");
  std::filesystem::remove(game.dir / "scripts/mover.rb");
  CHECK(settled_reload(host) == 0);
  CHECK(host.error_count() == errors + 3);
  CHECK(settled_reload(host) == 0);
  CHECK(host.error_count() == errors + 3);
  game.write(kChanged);
  CHECK(settled_reload(host) == 1);
  std::filesystem::rename(game.dir / "scripts", game.dir / "original_scripts");
  std::filesystem::create_directory_symlink(game.dir / "original_scripts", game.dir / "scripts");
  CHECK(settled_reload(host) == 0);
  CHECK(host.error_count() == errors + 4);
  CHECK(settled_reload(host) == 0);
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
  REQUIRE(settled_reload(host) == 1);
  const int errors = host.error_count();
  session->tick(config::kFixedDeltaSeconds);
  CHECK(host.error_count() == errors + 1);
  game.write(kChanged);
  REQUIRE(settled_reload(host) == 1);
  session->tick(config::kFixedDeltaSeconds);
  checked(host, "raise unless $actor.steps == $before + 5 && $starts == 1 && $fixed > 0");
  CHECK(host.error_count() == errors + 1);
  session->stop();
}
TEST_CASE("script reload: another file reopening a class keeps its methods and constants intact") {
  test::LogCapture quiet;
  ReloadGame game;
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  auto& host = session->runtime()->scripts();
  const auto helper = game.dir / "scripts/mover_ext.rb";
  std::string error;
  REQUIRE(write_text_file(helper, "class Mover; State = []; def boost; 77; end; end", error));
  REQUIRE(host.load_file(helper).ok);
  game.write(kChanged);
  CHECK(settled_reload(host) == 0);
  checked(host, "raise unless $actor.boost == 77 && Mover::State == []; raise unless Mover::STEP == 1");
  const auto logs = host.vm().reload_class(mrb_class_get(host.vm().state(), "Mover"), kChanged,
                                           (game.dir / "scripts/mover.rb").u8string());
  CHECK_FALSE(logs.ok);
  CHECK(logs.error.find("mover_ext.rb") != std::string::npos);
  checked(host, "GC.start; raise unless $actor.boost == 77");
}
TEST_CASE("script reload: a helper that defines the class first is named in the refusal") {
  RubyVm vm;
  checked(vm, "class Behaviour; end");
  vm.enable_reload_tracking();
  REQUIRE(vm.eval("class Mover < Behaviour; State = []; end", "helper.rb").ok);
  REQUIRE(vm.eval("class Mover; def value; 1; end; end", "mover.rb").ok);
  auto* klass = mrb_class_get(vm.state(), "Mover");
  const auto result = vm.reload_class(klass, "class Mover < Behaviour; def value; 9; end; end", "mover.rb");
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("helper.rb") != std::string::npos);
  checked(vm, "raise unless Mover.new.value == 1 && Mover::State == []");
}
TEST_CASE("script reload: failed starts stay disabled after replacing code") {
  test::LogCapture quiet;
  ReloadGame game;
  game.write(
      "class Mover < Behaviour; def start; raise 'no target'; end; def update(_dt); $bad_update = true; end; end");
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  auto& host = session->runtime()->scripts();
  const auto errors = host.error_count();
  REQUIRE(errors > 0);
  game.write("class Mover < Behaviour; def update(_dt); $bad_update = true; end; end");
  REQUIRE(settled_reload(host) == 1);
  session->tick(config::kFixedDeltaSeconds);
  checked(host, "raise if $bad_update");
  CHECK(host.error_count() == errors);
}
TEST_CASE("script reload: manual retry leaves successful unchanged constants and disabled callbacks alone") {
  test::LogCapture quiet;
  ReloadGame game;
  game.write(
      "class Mover < Behaviour; SPAWNED = []; def start; $actor = self; end; "
      "def update(_dt); raise 'broken'; end; end");
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  auto& host = session->runtime()->scripts();
  checked(host, "Mover::SPAWNED << 1; $constant_id = Mover::SPAWNED.object_id");
  const auto errors = host.error_count();
  REQUIRE(host.reload_scripts(true) == 0);
  session->tick(config::kFixedDeltaSeconds);
  CHECK(host.error_count() == errors);
  checked(host, "raise unless Mover::SPAWNED == [1] && Mover::SPAWNED.object_id == $constant_id");
}
TEST_CASE("script reload: CRLF loaded sources do not change at the first poll or manual retry") {
  ReloadGame game;
  std::string source;
  for (const char letter : std::string(kOriginal)) source += letter == '\n' ? "\r\n" : std::string(1, letter);
  game.write(source);
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  auto& host = session->runtime()->scripts();
  CHECK(settled_reload(host) == 0);
  CHECK(host.reload_scripts(true) == 0);
  CHECK(host.error_count() == 0);
}
TEST_CASE("script reload: incomplete writes settle before they can report an error") {
  ReloadGame game;
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  auto& host = session->runtime()->scripts();
  const auto errors = host.error_count();
  game.write("");
  CHECK(host.reload_scripts() == 0);
  CHECK(host.error_count() == errors);
  game.write("class Mover < Behaviour; def update(");
  CHECK(host.reload_scripts() == 0);
  CHECK(host.error_count() == errors);
  game.write(kChanged);
  CHECK(host.reload_scripts() == 0);
  CHECK(host.error_count() == errors);
  CHECK(host.reload_scripts() == 1);
  CHECK(host.error_count() == errors);
}
TEST_CASE("script reload: periodic content checks catch equal-size equal-timestamp changes") {
  ReloadGame game;
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  auto& host = session->runtime()->scripts();
  const auto path = game.dir / "scripts/mover.rb";
  const auto time = std::filesystem::last_write_time(path);
  std::string code = kOriginal;
  code.replace(code.find("STEP = 1"), std::string("STEP = 1").size(), "STEP = 9");
  game.write(code);
  std::filesystem::last_write_time(path, time);
  std::size_t reloaded = 0;
  for (int i = 0; i < 16; ++i) reloaded += host.reload_scripts();
  CHECK(reloaded == 1);
  checked(host, "raise unless Mover::STEP == 9");
}
TEST_CASE("script reload: plain Ruby declaration idioms and diagnostic locations are supported") {
  RubyVm vm;
  checked(vm, "class Behaviour; end; class Mover < Behaviour; end");
  auto* klass = mrb_class_get(vm.state(), "Mover");
  const auto result = vm.reload_class(klass, R"(
class Mover < ::Behaviour
  WORDS = %w[red blue]
  SYMBOLS = %i[up down]
  attr_reader "name"
  private def hidden; 42; end
end
)",
                                      "mover.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  checked(vm,
          "raise 'words' unless Mover::WORDS == ['red', 'blue']; "
          "raise 'symbols' unless Mover::SYMBOLS == [:up, :down]; "
          "raise 'reader' unless Mover.new.respond_to?(:name); "
          "raise 'body' unless Mover.new.send(:hidden) == 42");
  const auto hidden = mrb_method_search(vm.state(), klass, mrb_intern_lit(vm.state(), "hidden"));
  CHECK(MRB_METHOD_VISIBILITY(hidden) == MRB_METHOD_PRIVATE_FL);
  const auto invalid = vm.reload_class(klass, "class Mover < Behaviour\n VALUE = Object.new\nend", "mover.rb");
  CHECK_FALSE(invalid.ok);
  CHECK(invalid.error.find("line 2") != std::string::npos);
  CHECK(invalid.error.find("constant initializer") != std::string::npos);
}
TEST_CASE("script reload: a failed helper still owns the class changes made before it raised") {
  RubyVm vm;
  checked(vm, "class Behaviour; end");
  vm.enable_reload_tracking();
  REQUIRE(vm.eval("class Mover < Behaviour; def value; 1; end; end", "mover.rb").ok);
  CHECK_FALSE(vm.eval("class Mover; State = []; end; raise 'helper failed'", "broken_helper.rb").ok);
  auto* klass = mrb_class_get(vm.state(), "Mover");
  const auto result = vm.reload_class(klass, "class Mover < Behaviour; def value; 9; end; end", "mover.rb");
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("broken_helper.rb") != std::string::npos);
  checked(vm, "raise unless Mover.new.value == 1 && Mover::State == []");
}

TEST_CASE("script reload: ownership tracking preserves super inherited constants and class variables") {
  RubyVm vm;
  checked(vm, R"(
class Behaviour
  INHERITED = 7
  @@shared = 10
  def initialize; @base = 3; end
  def value; @base; end
  def self.shared; @@shared; end
end
)");
  vm.enable_reload_tracking();
  REQUIRE(vm.eval(R"(
class Mover < Behaviour
  def initialize; super; end
  def value; super + INHERITED + @@shared; end
  def bump; @@shared += 1; end
end
)",
                  "mover.rb")
              .ok);
  checked(vm, "$actor = Mover.new; raise unless $actor.value == 20; $actor.bump; raise unless Behaviour.shared == 11");
  mrb_full_gc(vm.state());
  auto* klass = mrb_class_get(vm.state(), "Mover");
  const auto result = vm.reload_class(klass, R"(
class Mover < Behaviour
  def value; super + INHERITED + @@shared + 1; end
end
)",
                                      "mover.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  checked(vm, "raise unless $actor.value == 22 && Behaviour.shared == 11");
}
TEST_CASE("script reload: the prelude keeps its own owner before any game file runs") {
  RubyVm vm;
  checked(vm, "class Behaviour; end; class NetworkSync < Behaviour; def engine_method; 42; end; end");
  vm.enable_reload_tracking();
  REQUIRE(vm.eval("class Mover < Behaviour; end", "mover.rb").ok);
  const auto result =
      vm.reload_class(mrb_class_get(vm.state(), "NetworkSync"), "class NetworkSync < Behaviour; end", "mover.rb");
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("engine prelude") != std::string::npos);
  checked(vm, "raise unless NetworkSync.new.engine_method == 42");
}
TEST_CASE("script reload: removing or replacing Behaviour does not invoke const_missing while observing") {
  for (const auto* replacement : {"", "Behaviour = 42"}) {
    RubyVm vm;
    checked(vm, "class Behaviour; end");
    vm.enable_reload_tracking();
    const auto result = vm.eval(std::string("Object.send(:remove_const, :Behaviour); ") + replacement, "game.rb");
    REQUIRE_MESSAGE(result.ok, result.error);
    checked(vm, "def Object.const_missing(name); raise 'unexpected autoload'; end; class Unrelated; end");
  }
}
TEST_CASE("script reload: unchanged initially oversized behaviours are excluded from watching") {
  test::LogCapture quiet;
  ReloadGame game;
  game.write(std::string(kOriginal) + "\n#" + std::string(kReloadSourceBytes, 'x') + "\n");
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  auto& host = session->runtime()->scripts();
  const auto errors = host.error_count();
  for (int i = 0; i < 20; ++i) CHECK(host.reload_scripts() == 0);
  CHECK(host.reload_scripts(true) == 0);
  game.write(kChanged);
  CHECK(host.reload_scripts(true) == 0);
  CHECK(host.error_count() == errors);
  checked(host, "raise unless Mover::STEP == 1");
}
namespace {
mrb_value load_nested_helper(mrb_state* mrb, mrb_value) {
  auto& vm = *static_cast<RubyVm*>(mrb->ud);
  const auto result = vm.eval("class Helper < Behaviour; def marker; 3; end; end", "helper.rb");
  if (!result.ok) mrb_raise(mrb, E_RUNTIME_ERROR, result.error.c_str());
  return mrb_obj_value(mrb_class_get(mrb, "Helper"));
}
}  // namespace
TEST_CASE("script reload: nested lazy evaluations retain the outermost source owner") {
  RubyVm vm;
  checked(vm, "class Behaviour; end");
  vm.enable_reload_tracking();
  vm.state()->ud = &vm;
  mrb_define_class_method(vm.state(), vm.state()->object_class, "const_missing", load_nested_helper, MRB_ARGS_REQ(1));
  REQUIRE(vm.eval("class Mover < Behaviour; HELPER = Helper; def value; 1; end; end", "mover.rb").ok);
  const auto helper = vm.reload_class(mrb_class_get(vm.state(), "Helper"),
                                      "class Helper < Behaviour; def marker; 9; end; end", "helper.rb");
  CHECK_FALSE(helper.ok);
  CHECK(helper.error.find("mover.rb") != std::string::npos);
  const auto mover = vm.reload_class(mrb_class_get(vm.state(), "Mover"),
                                     "class Mover < Behaviour; def value; 2; end; end", "mover.rb");
  REQUIRE_MESSAGE(mover.ok, mover.error);
  checked(vm, "raise unless Mover.new.value == 2 && Helper.new.marker == 3");
}
TEST_CASE("script reload: classes left by failed evaluation never acquire the next successful file as owner") {
  RubyVm vm;
  checked(vm, "class Behaviour; end");
  vm.enable_reload_tracking();
  CHECK_FALSE(vm.eval("class Mover < Behaviour; def original; 1; end; end; raise 'broken'", "broken.rb").ok);
  REQUIRE(vm.eval("class Mover; def added; 2; end; end", "mover.rb").ok);
  const auto result = vm.reload_class(mrb_class_get(vm.state(), "Mover"),
                                      "class Mover < Behaviour; def added; 3; end; end", "mover.rb");
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("broken.rb") != std::string::npos);
  checked(vm, "raise unless Mover.new.original == 1 && Mover.new.added == 2");
}

TEST_CASE("script reload: editor tracking leaves the prelude menu initializer callable") {
  test::LogCapture quiet;
  ReloadGame game;
  auto session = game.open();
  ScriptedInputSource input;
  start(*session, input);
  checked(session->runtime()->scripts(), "raise unless GameMenu.new.class == GameMenu");
}
