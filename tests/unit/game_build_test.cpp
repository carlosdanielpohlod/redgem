#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/bundled_game.hpp"
#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/translations.hpp"
#include "project/notices.hpp"
#include "project/android/android_package.hpp"
#include "project/game_build.hpp"

using namespace redgem;

namespace {

// A folder of the test's own, removed when the test ends, with a game and a folder that is none.
struct Scratch {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-game-build-test";
  std::filesystem::path game = dir / "game";
  std::filesystem::path empty = dir / "empty";
  Scratch() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(game);
    std::filesystem::create_directories(empty);
    std::ofstream(game / config::kSceneFileName) << "scene";
  }
  ~Scratch() { std::filesystem::remove_all(dir); }
};

}  // namespace

TEST_CASE("a folder is a game when it holds the scene a game starts with") {
  const Scratch scratch;
  CHECK(is_game_folder(scratch.game));
  CHECK_FALSE(is_game_folder(scratch.empty));
  CHECK_FALSE(is_game_folder(scratch.dir / "nowhere"));
}

// A build may read the game while the editor saves it: what the editor is still writing, under a
// name it takes off when done, is not part of the game.
TEST_CASE("a build ships no file the editor is still writing") {
  const Scratch scratch;
  std::ofstream(scratch.game / (std::string(config::kSceneFileName) + config::kSceneWritingSuffix)) << "half";
  std::ofstream(scratch.game / (std::string("player.rb") + config::kAgentWritingSuffix)) << "half";
  CHECK(shipped_files(scratch.game) == std::vector<std::filesystem::path>{config::kSceneFileName});
}

TEST_CASE("only a game is built, and what is not one is named") {
  const Scratch scratch;
  std::string error;
  CHECK(is_game_to_build(scratch.game, error));
  CHECK(error.empty());
  CHECK_FALSE(is_game_to_build(scratch.empty, error));
  CHECK(error == scratch.empty.string() + " is not a game (no scene.glb)");
}

TEST_CASE("a build goes anywhere but into the game it copies") {
  const Scratch scratch;
  std::string error;
  CHECK(builds_outside_game(scratch.game, scratch.dir / "out", error));
  CHECK(error.empty());
  CHECK_FALSE(builds_outside_game(scratch.game, scratch.game / "out", error));
  CHECK(error == "cannot build into " + (scratch.game / "out").string() + ": it is inside the game folder");
  CHECK_FALSE(builds_outside_game(scratch.game, scratch.game, error));
}

// The game folder of a build for a computer is emptied on each build: the game must not be in it.
// An APK replaces no folder there, so it may go beside the game.
TEST_CASE("a build for a computer never goes where its game folder would be the game being built") {
  const Scratch scratch;  // its game is <dir>/game, which a build into <dir> would replace
  std::string error;
  CHECK(builds_outside_game(scratch.game, scratch.dir, error));
  std::ofstream(scratch.dir / "player") << "player";
  CHECK_FALSE(build_game(scratch.game, scratch.dir, scratch.dir / "player", BuildTarget::Linux, error));
  CHECK(error == tr("project.build.game_inside_its_build",
                    {{"game", scratch.game.string()}, {"folder", (scratch.dir / kBundledGameFolder).string()}}));
  CHECK(std::filesystem::exists(scratch.game / config::kSceneFileName));
}

// A build writes the executable, the game folder and the licenses: over what an earlier build
// wrote, never over a file or folder of one of those names that someone else put there.
TEST_CASE("a build replaces only what an earlier build made") {
  const Scratch scratch;
  const std::filesystem::path player = scratch.dir / "player";
  std::ofstream(player) << "player";
  std::string error;
  const std::filesystem::path out = scratch.dir / "out";
  REQUIRE_MESSAGE(build_game(scratch.game, out, player, BuildTarget::Linux, error), error);
  REQUIRE_MESSAGE(build_game(scratch.game, out, player, BuildTarget::Linux, error), error);  // its own: replaced
  REQUIRE_MESSAGE(build_game(scratch.game, out, player, BuildTarget::Windows, error), error);

  const std::string executable = executable_name(scratch.game);  // for Linux
  for (const std::string& name : {std::string(kBundledGameFolder), executable, std::string(kNoticesFolder)}) {
    CAPTURE(name);
    const std::filesystem::path other_out = scratch.dir / ("other_out_" + std::to_string(name.size()));
    std::filesystem::remove_all(other_out);
    std::filesystem::create_directories(other_out);
    std::ofstream(other_out / name) << "someone's own";
    CHECK_FALSE(build_game(scratch.game, other_out, player, BuildTarget::Linux, error));
    CHECK(error == tr("project.build.not_a_build", {{"path", (other_out / name).string()}}));
    CHECK(std::filesystem::is_regular_file(other_out / name));
  }
}

#ifndef _WIN32  // (a folder that cannot be emptied, made by taking away the right to change it)
TEST_CASE("a build whose earlier game folder cannot be emptied fails instead of shipping what is left") {
  const Scratch scratch;
  const std::filesystem::path player = scratch.dir / "player";
  std::ofstream(player) << "player";
  const std::filesystem::path out = scratch.dir / "out";
  std::string error;
  REQUIRE_MESSAGE(build_game(scratch.game, out, player, BuildTarget::Linux, error), error);
  const std::filesystem::path stuck = out / kBundledGameFolder / "stuck";
  std::filesystem::create_directories(stuck);
  std::ofstream(stuck / "stale.txt") << "from before";
  std::filesystem::permissions(stuck, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
  CHECK_FALSE(build_game(scratch.game, out, player, BuildTarget::Linux, error));
  CHECK(error.find("cannot empty") != std::string::npos);
  std::filesystem::permissions(stuck, std::filesystem::perms::owner_all);
}
#endif

TEST_CASE("a player no script of the engine makes is said to come from building the engine on its system") {
  for (const BuildTarget target : {BuildTarget::Linux, BuildTarget::MacOS}) {
    if (target == host_build_target()) continue;  // (the one built here is its own build's target)
    const std::string name = build_target_name(target);
    const std::string message = missing_runtime_message(target, "/opt/redgem/bin/runtimes/" + name);
    CAPTURE(message);
    CHECK(message.find("building the engine on " + name) != std::string::npos);
    CHECK(message.find("build_runtime.sh") == std::string::npos);
  }
}

TEST_CASE("a missing player for Android is looked for under any kind of processor") {
  CHECK(any_android_runtime_in("runtimes/android") ==
        std::filesystem::path("runtimes/android") / "<abi>" / android_library_file());
}
