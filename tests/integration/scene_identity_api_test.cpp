#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>
#include "app/engine.hpp"
#include "core/config.hpp"
#include "storage/save_file.hpp"
#include "test_support.hpp"

using namespace redgem;
namespace {
struct IdentityGame {
  SaveFile save;
  std::unique_ptr<Engine> engine;
  explicit IdentityGame(SaveStore* shared = nullptr) {
    EngineOptions options;
    options.game_dir = test::fixture("games/sandbox");
    options.save = shared != nullptr ? shared : &save;
    options.fixed_step = true;
    auto made = Engine::create(options);
    REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(made));
    engine = std::move(std::get<std::unique_ptr<Engine>>(made));
  }
  std::string ruby(const char* code) {
    const auto result = engine->scripts().eval(code, "identity_test.rb");
    REQUIRE_MESSAGE(result.ok, result.error);
    return RSTRING_PTR(mrb_obj_as_string(engine->scripts().vm().state(), result.value));
  }
};
}  // namespace

TEST_CASE("scripts keep file identities in Save and resolve them after an engine restart") {
  SaveFile shared;
  std::string identity;
  {
    IdentityGame game(&shared);
    identity = game.ruby("$object = Scene.objects.first; $object.stable_id");
    CHECK_FALSE(identity.empty());
    game.ruby("Save[:object_id] = $object.stable_id");
    CHECK(game.ruby("Scene.find_by_id(Save[:object_id]) == $object") == "true");
    CHECK(game.ruby("Scene.find_by_id($object.stable_id.to_sym) == $object") == "true");
  }
  IdentityGame restarted(&shared);
  CHECK(restarted.ruby("Scene.find_by_id(Save[:object_id]).stable_id") == identity);
}

TEST_CASE("script lookup refuses unknown identities and does not bind clones to an original") {
  IdentityGame game;
  game.ruby("$original = Scene.objects.first; $copy = Scene.spawn($original, physics: false)");
  CHECK(game.ruby("$copy.stable_id.nil?") == "true");
  CHECK(game.ruby("Scene.find_by_id($original.stable_id) == $original") == "true");
  CHECK(game.ruby("Scene.find_by_id('missing').nil? && Scene.find_by_id('').nil?") == "true");
  CHECK(game.ruby("begin; Scene.find_by_id(7); rescue TypeError; 'rejected'; end") == "rejected");
}

TEST_CASE("a stored scene identity never revives a stale reference after switching scenes") {
  IdentityGame game;
  game.ruby("$old = Scene.objects.first; $id = $old.stable_id; Game.load_scene('level2')");
  game.engine->frame(config::kFixedDeltaSeconds);
  CHECK(game.ruby("!$old.alive?") == "true");
  CHECK(game.ruby("begin; $old.stable_id; rescue RuntimeError; 'gone'; end") == "gone");
  CHECK(game.ruby("$current = Scene.find_by_id($id); $current.nil? || $current != $old") == "true");
}
