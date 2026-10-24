// A game's actions read from its game.json, and its players' bindings kept between runs.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/input_bindings_store.hpp"
#include "app/save_location.hpp"
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

TEST_CASE("a game's players keep their bindings named as its save is: by its id, else by its folder") {
  const TempFolder game;
  const std::filesystem::path bindings = platform::user_data_dir() / "bindings";
  CHECK(bindings_file_for(game.dir) == game_file_in(bindings, game.dir));
  CHECK(bindings_file_for(game.dir).parent_path() == bindings);
  CHECK(bindings_file_for(game.dir).filename().string().rfind(game.dir.filename().string(), 0) == 0);
  CHECK(save_file_for(game.dir) == game_file_in(saves_folder(), game.dir));
  game.write("game.json", R"({"id": "com.studio.racer"})");
  CHECK(bindings_file_for(game.dir) == bindings / "by-id" / "com.studio.racer.json");
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

TEST_CASE("a saved change with an unknown binding leaves that name on the game's binding") {
  const TempFolder folder;
  folder.write("partly.json", R"({"pads": [{"actions": {"jump": ["key:k", "key:hyper"], "fire": ["key:f"]}}]})");
  PlayerBindings bindings;
  read_player_bindings(folder.dir / "partly.json", bindings);
  CHECK(listed(bindings.action(0, "jump")) == std::vector<InputBinding>{Key::Space, GamepadButton::A});
  CHECK(listed(bindings.action(0, "fire")) == std::vector<InputBinding>{Key::F});
}

TEST_CASE("a player's axis is saved without its stick") {
  const TempFolder folder;
  PlayerBindings bindings;
  bindings.bind(0, "vertical", Key::I, AxisSide::Positive);
  const std::string text = player_bindings_text(bindings);
  CHECK(text.find("pad_axis") == std::string::npos);
  CHECK(text.find("invert") == std::string::npos);
  CHECK(text.find("key:i") != std::string::npos);
}
