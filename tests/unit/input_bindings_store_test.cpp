// A game's actions read from its game.json, and its players' bindings kept between runs.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/input_bindings_store.hpp"
#include "core/platform.hpp"

using namespace redgem;

namespace {

constexpr std::size_t kThirdPad = 2;

// A folder of the test's own, removed when the test ends.
struct TempFolder {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-input-bindings-store-test";
  TempFolder() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
  }
  ~TempFolder() { std::filesystem::remove_all(dir); }
  TempFolder(const TempFolder&) = delete;
  TempFolder& operator=(const TempFolder&) = delete;

  void write(const std::string& name, const std::string& text) const { std::ofstream(dir / name) << text; }
};

std::vector<InputBinding> listed(const ActionBinding* binding) {
  return binding == nullptr ? std::vector<InputBinding>{} : binding->all();
}

}  // namespace

TEST_CASE("a game declares actions in game.json's input section") {
  const TempFolder game;
  std::vector<std::string> problems;
  CHECK(read_game_input(game.dir, problems).empty());  // no game.json at all
  game.write("game.json", R"({"title": "Racer", "input": {"actions": {"boost": ["key:b", "pad:right_bumper"]},
      "axes": {"steer": {"negative": ["key:a"], "positive": ["key:d"], "pad_axis": "left_x"}}}})");
  const InputMap map = read_game_input(game.dir, problems);
  CHECK(problems.empty());
  CHECK(listed(map.action("boost")) == std::vector<InputBinding>{Key::B, GamepadButton::RightBumper});
  REQUIRE(map.axis("steer") != nullptr);
  CHECK(map.axis("steer")->stick == GamepadAxis::LeftX);
  game.write("game.json", R"({"title": "Racer"})");
  CHECK(read_game_input(game.dir, problems).empty());
  CHECK(problems.empty());
}

TEST_CASE("a game's players keep their bindings by the game's id; a game with none keeps them nowhere") {
  const TempFolder game;
  CHECK(bindings_file_for(game.dir).empty());
  game.write("game.json", R"({"id": "com.studio.racer"})");
  CHECK(bindings_file_for(game.dir) == platform::user_data_dir() / "bindings" / "com.studio.racer.json");
}

TEST_CASE("what players bound is written and read back, player by player") {
  const TempFolder folder;
  const std::filesystem::path file = folder.dir / "nested" / "bindings.json";
  PlayerBindings played;
  played.declare_action("dash", {{Key::E}, {}});
  played.bind(0, "jump", Key::K, std::nullopt);
  played.unbind(0, "dash", std::nullopt, std::nullopt);
  played.bind(kThirdPad, "horizontal", GamepadButton::DpadLeft, AxisSide::Negative);
  REQUIRE(write_player_bindings(file, played));

  PlayerBindings next_run;
  read_player_bindings(file, next_run);
  next_run.declare_action("dash", {{Key::E}, {}});  // (the game declares it after the file is read)
  CHECK(listed(next_run.action(0, "jump")) == listed(played.action(0, "jump")));
  CHECK(listed(next_run.action(0, "dash")).empty());
  CHECK(next_run.axis(kThirdPad, "horizontal")->negative.all() == std::vector<InputBinding>{GamepadButton::DpadLeft});
  CHECK(next_run.axis(kThirdPad, "horizontal")->stick == GamepadAxis::LeftX);
  CHECK(next_run.changes(1).empty());
  CHECK_FALSE(std::filesystem::exists(file.string() + ".part"));
}

TEST_CASE("a missing or broken bindings file leaves every player with the game's bindings") {
  const TempFolder folder;
  PlayerBindings bindings;
  read_player_bindings(folder.dir / "none.json", bindings);
  folder.write("broken.json", "{\"pads\": [{\"actions\": ");
  read_player_bindings(folder.dir / "broken.json", bindings);
  folder.write("odd.json", R"({"pads": {"0": {}}})");
  read_player_bindings(folder.dir / "odd.json", bindings);
  for (std::size_t pad = 0; pad < kMaxGamepads; ++pad) CHECK(bindings.changes(pad).empty());
  CHECK(listed(bindings.action(0, "jump")) == std::vector<InputBinding>{Key::Space, GamepadButton::A});
}
