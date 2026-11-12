// A built game whose files are packed into one archive (game.rgpak) plays as it does from its
// project folder: the platformer demo, built, opened as the player opens it, and played headless.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "app/bundled_game.hpp"
#include "app/engine.hpp"
#include "app/game_settings.hpp"
#include "core/config.hpp"
#include "core/game_archive.hpp"
#include "core/game_files.hpp"
#include "project/game_build.hpp"
#include "script/game_locales.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr int kPlayFrames = 120;

// A folder of the test's own with a stand-in for the player, removed (and the archive unmounted)
// when the test ends.
struct Scratch {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-packed-game-test";
  std::filesystem::path out = dir / "out";
  std::filesystem::path player = dir / "player";
  Scratch() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::ofstream(player) << "player";
  }
  ~Scratch() {
    game_files::unmount();
    std::filesystem::remove_all(dir);
  }
  std::filesystem::path executable() const { return out / executable_name(REDGEM_PLATFORMER_DIR); }
};

std::unique_ptr<Engine> play_headless(const std::filesystem::path& game) {
  EngineOptions options;
  options.game_dir = game;
  options.fixed_step = true;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

// How many behaviours the game's scripts attached, counted the way a game would count them.
int behaviour_count(Engine& engine) {
  const RubyResult result =
      engine.scripts().eval("Scene.objects.inject(0) { |total, object| total + object.behaviours.size }", "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return std::stoi(RSTRING_PTR(mrb_obj_as_string(mrb, result.value)));
}

// Plays a game for a while; how many behaviours it runs, checking it ran without a script error.
int played(const std::filesystem::path& game) {
  auto engine = play_headless(game);
  for (int i = 0; i < kPlayFrames; ++i) engine->frame(config::kFixedDeltaSeconds);
  CHECK(engine->scripts().error_count() == 0);
  return behaviour_count(*engine);
}

}  // namespace

TEST_CASE("the platformer built into an archive plays as it does from its folder") {
  const Scratch scratch;
  std::string error;
  REQUIRE_MESSAGE(build_game(REDGEM_PLATFORMER_DIR, scratch.out, scratch.player, BuildTarget::Linux, error), error);
  REQUIRE(std::filesystem::is_regular_file(scratch.out / kArchiveFileName));
  CHECK_FALSE(std::filesystem::exists(scratch.out / kBundledGameFolder));
  const int loose_behaviours = played(REDGEM_PLATFORMER_DIR);
  REQUIRE(loose_behaviours > 0);

  const auto game = open_bundled_game(scratch.executable(), error);
  REQUIRE_MESSAGE(game.has_value(), error);
  CHECK(*game == scratch.out / kBundledGameFolder);
  CHECK_FALSE(std::filesystem::exists(*game));   // nothing of it is on the disk
  CHECK(read_game_manifest(*game).file_parsed);  // game.json, read out of the archive
  CHECK(read_game_settings(*game).studio == read_game_settings(REDGEM_PLATFORMER_DIR).studio);
  CHECK(read_game_locales(*game).languages == read_game_locales(REDGEM_PLATFORMER_DIR).languages);
  CHECK(played(*game) == loose_behaviours);  // every script found, loaded and run from the archive
}

TEST_CASE("a build kept loose is played from its folder, and the next packed build takes that folder away") {
  const Scratch scratch;
  std::string error;
  REQUIRE_MESSAGE(
      build_game(REDGEM_PLATFORMER_DIR, scratch.out, scratch.player, BuildTarget::Linux, error, GameLayout::Loose),
      error);
  CHECK(std::filesystem::exists(scratch.out / kBundledGameFolder / config::kSceneFileName));
  CHECK_FALSE(std::filesystem::exists(scratch.out / kArchiveFileName));
  const auto loose = open_bundled_game(scratch.executable(), error);
  REQUIRE_MESSAGE(loose.has_value(), error);
  CHECK_FALSE(game_files::mounted_root().has_value());

  REQUIRE_MESSAGE(build_game(REDGEM_PLATFORMER_DIR, scratch.out, scratch.player, BuildTarget::Linux, error), error);
  CHECK(std::filesystem::exists(scratch.out / kArchiveFileName));
  CHECK_FALSE(std::filesystem::exists(scratch.out / kBundledGameFolder));
}

TEST_CASE("a damaged archive beside the player is refused with a reason, and no game is played") {
  const Scratch scratch;
  std::filesystem::create_directories(scratch.out);
  std::ofstream(scratch.out / kArchiveFileName) << "not an archive at all, though long enough to have a header";
  std::string error;
  CHECK_FALSE(open_bundled_game(scratch.executable(), error).has_value());
  CHECK(error.find("damaged") != std::string::npos);
  CHECK(error.find(kArchiveFileName) != std::string::npos);
  CHECK_FALSE(game_files::mounted_root().has_value());
  std::filesystem::remove(scratch.out / kArchiveFileName);
  CHECK_FALSE(open_bundled_game(scratch.executable(), error).has_value());
  CHECK(error.find("no game to play") != std::string::npos);
}
