#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "core/config.hpp"
#include "project/build_steps.hpp"
#include "project/game_build.hpp"

using namespace redgem;

namespace {

constexpr const char* kStep = "polishing the gems";
constexpr const char* kOtherStep = "counting the gems";

// A folder of the test's own with a game and a player to ship, removed when the test ends.
struct Scratch {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-build-steps-test";
  std::filesystem::path game = dir / "game";
  std::filesystem::path player = dir / "player";
  Scratch() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(game);
    std::ofstream(game / config::kSceneFileName) << "scene";
    std::ofstream(player) << "player";
  }
  ~Scratch() { std::filesystem::remove_all(dir); }
};

}  // namespace

TEST_CASE("a build's steps reach whoever listens on its thread, and nobody once they stop") {
  std::vector<std::string> heard;
  build_steps::begin(kStep);  // nobody listens yet: nothing happens
  {
    const build_steps::Listen listen([&heard](const std::string& step) { heard.push_back(step); });
    build_steps::begin(kStep);
  }
  build_steps::begin(kOtherStep);
  CHECK(heard == std::vector<std::string>{kStep});
}

TEST_CASE("a listener inside another hears the steps until it goes, then the outer one does again") {
  std::vector<std::string> outer;
  std::vector<std::string> inner;
  const build_steps::Listen listen_outer([&outer](const std::string& step) { outer.push_back(step); });
  {
    const build_steps::Listen listen_inner([&inner](const std::string& step) { inner.push_back(step); });
    build_steps::begin(kStep);
  }
  build_steps::begin(kOtherStep);
  CHECK(inner == std::vector<std::string>{kStep});
  CHECK(outer == std::vector<std::string>{kOtherStep});
}

TEST_CASE("a build on another thread is not heard by who listens on this one") {
  std::vector<std::string> heard;
  const build_steps::Listen listen([&heard](const std::string& step) { heard.push_back(step); });
  std::thread([] { build_steps::begin(kStep); }).join();
  CHECK(heard.empty());
}

TEST_CASE("a build for a computer says it copies the player, then the game's files, then writes its icon") {
  const Scratch scratch;
  std::vector<std::string> heard;
  const build_steps::Listen listen([&heard](const std::string& step) { heard.push_back(step); });
  std::string error;
  REQUIRE_MESSAGE(build_game(scratch.game, scratch.dir / "out", scratch.player, BuildTarget::Linux, error), error);
  CHECK(heard ==
        std::vector<std::string>{build_steps::kCopyingPlayer, build_steps::kCopyingFiles, build_steps::kShippingFiles});
}
