#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>
#include <filesystem>
#include "app/engine.hpp"
#include "core/config.hpp"
#include "server/save_database.hpp"
#include "test_support.hpp"
#include "storage/save_file.hpp"
#include "storage/save_view.hpp"
using namespace redgem;
namespace {
struct SlotGame {
  std::unique_ptr<Engine> engine;
  SlotGame(const std::filesystem::path& path, SaveStore* shared = nullptr) {
    EngineOptions options;
    options.game_dir = test::fixture("games/sandbox");
    options.save_file = path;
    options.save = shared;
    options.fixed_step = true;
    auto made = Engine::create(options);
    REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(made));
    engine = std::move(std::get<std::unique_ptr<Engine>>(made));
  }
  std::string ruby(const char* code) {
    const auto result = engine->scripts().eval(code, "slot_test.rb");
    REQUIRE_MESSAGE(result.ok, result.error);
    return RSTRING_PTR(mrb_obj_as_string(engine->scripts().vm().state(), result.value));
  }
  void frame() { engine->frame(config::kFixedDeltaSeconds); }
};
struct SlotData {
  std::filesystem::path folder = std::filesystem::temp_directory_path() / "redgem-slot-api-test";
  std::filesystem::path file = folder / "save.json";
  SlotData() { std::filesystem::remove_all(folder); }
  ~SlotData() { std::filesystem::remove_all(folder); }
};
}  // namespace
TEST_CASE("Save selects isolated named slots and keeps the default across restarts") {
  SlotData data;
  {
    SlotGame game(data.file);
    CHECK(game.ruby("Save.slot") == "default");
    game.ruby("Save[:coins] = 7; Save.slot = :campaign; Save[:coins] = 2");
    game.ruby("Save.slot = 'other'; Save[:coins] = 9; Save.slot = 'campaign'; Save[:level] = 3");
    CHECK(game.ruby("Save[:coins]") == "2");
    CHECK(game.ruby("Save.slots == ['campaign', 'default', 'other']") == "true");
    game.ruby("Game.load_scene('level2')");
    game.frame();
    game.frame();
    CHECK(game.ruby("Save.slot") == "campaign");
    CHECK(game.ruby("Save[:level]") == "3");
    game.ruby("Save.slot = 'default'");
    CHECK(game.ruby("Save[:coins]") == "7");
  }
  SlotGame reopened(data.file);
  CHECK(reopened.ruby("Save.slot") == "default");
  CHECK(reopened.ruby("Save.slots == ['campaign', 'default', 'other']") == "true");
  CHECK(reopened.ruby("Save.slot = 'campaign'; Save[:level]") == "3");
}
TEST_CASE("invalid slot names preserve the current selection and data") {
  SlotData data;
  SlotGame game(data.file);
  game.ruby("Save.slot = 'campaign'; Save[:coins] = 3");
  CHECK(game.ruby("begin; Save.slot = '../bad'; rescue ArgumentError; 'rejected'; end") == "rejected");
  CHECK(game.ruby("Save.slot") == "campaign");
  CHECK(game.ruby("Save[:coins]") == "3");
  CHECK(game.ruby("begin; Save.slot = 'MixedCase'; rescue ArgumentError; 'rejected'; end") == "rejected");
}
TEST_CASE("rooms choose save slots independently while a block update stays on its original store") {
  SlotData data;
  std::string error;
  auto shared = server::SaveDatabase::open(data.folder / "save.sqlite", error);
  REQUIRE_MESSAGE(shared, error);
  SlotGame one(data.file, shared.get()), two(data.file, shared.get());
  one.ruby("Save.slot = 'campaign'; Save[:coins] = 3");
  two.ruby("Save.slot = 'other'; Save[:coins] = 9");
  CHECK(one.ruby("Save[:coins]") == "3");
  CHECK(two.ruby("Save[:coins]") == "9");
  one.ruby("Save.update(:coins) { |value| Save.slot = 'other'; value + 1 }");
  CHECK(one.ruby("Save.slot") == "other");
  CHECK(one.ruby("Save[:coins]") == "9");
  CHECK(two.ruby("Save.slot = 'campaign'; Save[:coins]") == "4");
  one.ruby("Save.slot = 'default'");
  CHECK(one.ruby("Save[:coins].nil?") == "true");
}

TEST_CASE("a supplied backend without slots retains its default data on a rejected selection") {
  SlotData data;
  SaveFile memory;
  SaveView custom(memory);
  SlotGame game(data.file, &custom);
  game.ruby("Save[:coins] = 7");
  CHECK(game.ruby("begin; Save.slot = 'campaign'; rescue RuntimeError; 'unsupported'; end") == "unsupported");
  CHECK(game.ruby("Save.slot") == "default");
  CHECK(game.ruby("Save[:coins]") == "7");
}
