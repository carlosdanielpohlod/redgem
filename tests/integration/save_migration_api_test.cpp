// Save.version and Save.migrate: a patched game's hooks bring an older slot up to date once, and a
// slot it cannot read (newer, or a hook failed on it) is refused and left as it was.

#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>
#include <filesystem>
#include <string>
#include "app/engine.hpp"
#include "core/config.hpp"
#include "server/save_database.hpp"
#include "test_support.hpp"

using namespace redgem;
namespace {
constexpr const char* kVersionThree = R"ruby(
  $ran = []
  Save.version = 3
  Save.migrate(from: 1) { |data| $ran << 1; data.merge('coins' => data.delete('gold')) }
  Save.migrate(from: 2) { |data| $ran << 2; data.merge('lives' => 3) }
)ruby";
constexpr const char* kRefused = "begin; Save[:gold]; 'loaded'; rescue RuntimeError => e; e.message; end";

struct MigrationGame {
  std::unique_ptr<Engine> engine;
  explicit MigrationGame(const std::filesystem::path& path, SaveStore* shared = nullptr) {
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
    const auto result = engine->scripts().eval(code, "migration_test.rb");
    REQUIRE_MESSAGE(result.ok, result.error);
    return RSTRING_PTR(mrb_obj_as_string(engine->scripts().vm().state(), result.value));
  }
};
struct MigrationData {
  std::filesystem::path folder = std::filesystem::temp_directory_path() / "redgem-save-migration-test";
  std::filesystem::path file = folder / "save.json";
  MigrationData() { std::filesystem::remove_all(folder); }
  ~MigrationData() { std::filesystem::remove_all(folder); }
};
bool contains(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }
}  // namespace

TEST_CASE("migrations run in order on an older slot, and only once") {
  MigrationData data;
  MigrationGame(data.file).ruby("Save[:gold] = 5");  // a slot from before the game had a version
  {
    MigrationGame patched(data.file);
    patched.ruby(kVersionThree);
    CHECK(patched.ruby("Save[:coins]") == "5");
    CHECK(patched.ruby("Save[:lives]") == "3");
    CHECK(patched.ruby("Save[:gold].nil?") == "true");
    CHECK(patched.ruby("$ran") == "[1, 2]");
    patched.ruby("Save[:coins] = 6");
    CHECK(patched.ruby("$ran") == "[1, 2]");
    CHECK(patched.ruby("Save.keys") == "[:coins, :lives]");
  }
  MigrationGame again(data.file);
  again.ruby(kVersionThree);
  CHECK(again.ruby("Save[:coins]") == "6");
  CHECK(again.ruby("$ran") == "[]");
}

TEST_CASE("a slot first written by a versioned game skips the hooks of older versions") {
  MigrationData data;
  MigrationGame(data.file).ruby("Save.version = 2; Save[:gold] = 5");
  MigrationGame patched(data.file);
  patched.ruby(kVersionThree);
  CHECK(patched.ruby("Save[:gold]") == "5");
  CHECK(patched.ruby("Save[:lives]") == "3");
  CHECK(patched.ruby("$ran") == "[2]");
}

TEST_CASE("a slot saved by a newer game is refused and left as it was") {
  MigrationData data;
  MigrationGame(data.file).ruby("Save.version = 4; Save[:gold] = 5; Save.slot = 'campaign'; Save[:gold] = 1");
  {
    MigrationGame older(data.file);
    older.ruby("Save.version = 2");
    CHECK(contains(older.ruby(kRefused), "newer version of the game (save version 4, this one is 2)"));
    CHECK(older.ruby("begin; Save[:gold] = 9; rescue RuntimeError; 'refused'; end") == "refused");
    CHECK(older.ruby("begin; Save.slot = 'campaign'; rescue RuntimeError; 'refused'; end") == "refused");
    CHECK(older.ruby("Save.slot") == "default");
  }
  MigrationGame newest(data.file);
  newest.ruby("Save.version = 4");
  CHECK(newest.ruby("Save[:gold]") == "5");
  CHECK(newest.ruby("Save.slot = 'campaign'; Save[:gold]") == "1");
}

TEST_CASE("a failing migration hook leaves the slot untouched and says why") {
  MigrationData data;
  MigrationGame(data.file).ruby("Save[:gold] = 5");
  {
    MigrationGame patched(data.file);
    patched.ruby("Save.version = 3; Save.migrate(from: 1) { |data| data.merge('a' => 1) }");
    patched.ruby("Save.migrate(from: 2) { |data| raise 'broken hook' }");
    const auto refusal = patched.ruby(kRefused);
    CHECK(contains(refusal, "could not be migrated to save version 3"));
    CHECK(contains(refusal, "broken hook"));
  }
  MigrationGame unpatched(data.file);
  CHECK(unpatched.ruby("Save.keys") == "[:gold]");
  CHECK(unpatched.ruby("Save[:gold]") == "5");
  CHECK(unpatched.ruby("Save.__get(:__save_version).nil?") == "true");
}

TEST_CASE("a hook must give the new values back as a Hash") {
  MigrationData data;
  MigrationGame(data.file).ruby("Save[:gold] = 5");
  MigrationGame patched(data.file);
  patched.ruby("Save.version = 2; Save.migrate(from: 1) { |data| nil }");
  CHECK(contains(patched.ruby(kRefused), "must return the new values as a Hash"));
  CHECK(patched.ruby("begin; Save.version = 0; rescue ArgumentError; 'bad'; end") == "bad");
  CHECK(patched.ruby("begin; Save.migrate(from: 1) { |d| d }; rescue ArgumentError; 'twice'; end") == "twice");
}

TEST_CASE("a server's database slot is migrated in one transaction") {
  MigrationData data;
  std::string error;
  auto shared = server::SaveDatabase::open(data.folder / "save.sqlite", error);
  REQUIRE_MESSAGE(shared, error);
  MigrationGame(data.file, shared.get()).ruby("Save.slot = 'campaign'; Save[:gold] = 5; Save[:old] = true");
  MigrationGame patched(data.file, shared.get());
  patched.ruby(kVersionThree);
  CHECK(patched.ruby("Save.slot = 'campaign'; Save.keys") == "[:coins, :lives, :old]");
  CHECK(patched.ruby("Save[:coins]") == "5");
  CHECK(patched.ruby("$ran") == "[1, 2]");
}
