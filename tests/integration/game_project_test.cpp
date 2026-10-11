#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include <filesystem>

#include "app/engine.hpp"
#include "app/game_project.hpp"
#include "audio/audio.hpp"
#include "core/config.hpp"
#include "editor/script_files.hpp"
#include "scene/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr int kWalkFrames = 90;
constexpr float kMinWalk = 2.0f;
constexpr std::uintmax_t kMinToneBytes = 1000;

std::filesystem::path fresh(const std::string& name) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("redgem_project_" + name);
  std::filesystem::remove_all(dir);
  return dir;
}

std::unique_ptr<Engine> play_headless(const std::filesystem::path& game) {
  EngineOptions options;
  options.game_dir = game;
  options.headless = true;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

}  // namespace

TEST_CASE("every template creates a game that loads and whose scripts compile") {
  for (const std::string& name : game_templates()) {
    CAPTURE(name);
    const auto dir = fresh("template_" + name);
    std::string error;
    REQUIRE_MESSAGE(create_game(dir, name, error), error);
    const LoadResult loaded = load_gltf_scene((dir / config::kSceneFileName).string());
    REQUIRE_MESSAGE(loaded.ok, loaded.error);
    CHECK(loaded.scene.find_node("Camera") != kNoIndex);
    CHECK(loaded.scene.find_node("Sun") != kNoIndex);
    for (const char* folder : {config::kScriptsDirName, config::kSoundsDirName, config::kScenesDirName,
                               config::kModelsDirName}) {
      CHECK(std::filesystem::is_directory(dir / folder));
    }
    RubyVm vm;
    std::size_t scripts = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir / config::kScriptsDirName)) {
      const auto code = read_text_file(entry.path(), error);
      REQUIRE(code);
      CHECK_MESSAGE(vm.check_syntax(*code, entry.path().filename().string()).ok, entry.path());
      ++scripts;
    }
    CHECK(scripts > 0);
    CHECK(read_game_settings(dir).title == dir.filename().string());
    test::LogCapture quiet;
    auto engine = play_headless(dir);
    engine->frame(config::kFixedDeltaSeconds);
    CHECK(engine->scripts().error_count() == 0);
    std::filesystem::remove_all(dir);
  }
}

TEST_CASE("the platformer template is playable") {
  const auto dir = fresh("platformer_play");
  std::string error;
  REQUIRE(create_game(dir, "platformer", error));
  auto engine = play_headless(dir);
  const ObjectId player = engine->scene().find("Player");
  REQUIRE(player != kNoObject);
  const Vec3 start = engine->scene().world_transform(player).position;
  engine->scripted_input().set_key(Key::W, true);  // forward, through the first coin

  // The game opens on its title screen: nothing moves until Play is clicked.
  for (int i = 0; i < kWalkFrames; ++i) engine->frame(config::kFixedDeltaSeconds);
  CHECK(length(engine->scene().world_transform(player).position - start) < kMinWalk);
  const RubyResult title = engine->scripts().eval("Scene.find('Menu').props[:title]", "title.rb");
  REQUIRE(title.ok);
  CHECK(std::string(RSTRING_PTR(title.value)) == dir.filename().string());  // the game's name
  const RubyResult play = engine->scripts().eval(
      "box = GameMenu.instance.button_box(0); [box[0] + box[2] / 2, box[1] + box[3] / 2]", "play_button.rb");
  REQUIRE_MESSAGE(play.ok, play.error);
  mrb_state* mrb = engine->scripts().vm().state();
  engine->scripted_input().move_mouse(static_cast<float>(mrb_as_float(mrb, mrb_ary_entry(play.value, 0))),
                                      static_cast<float>(mrb_as_float(mrb, mrb_ary_entry(play.value, 1))));
  engine->scripted_input().set_mouse_button(MouseButton::Left, true);
  engine->frame(config::kFixedDeltaSeconds);
  engine->scripted_input().set_mouse_button(MouseButton::Left, false);
  engine->frame(config::kFixedDeltaSeconds);

  for (int i = 0; i < kWalkFrames; ++i) engine->frame(config::kFixedDeltaSeconds);
  CHECK(length(engine->scene().world_transform(player).position - start) > kMinWalk);
  const RubyResult coins = engine->scripts().eval("Scene.find('Player').get_component(Player).coins", "coins.rb");
  REQUIRE(coins.ok);
  CHECK(mrb_integer(coins.value) >= 1);
  CHECK(engine->scripts().error_count() == 0);
  std::filesystem::remove_all(dir);
}

TEST_CASE("games are not created over existing work or from unknown templates") {
  const auto dir = fresh("busy");
  std::filesystem::create_directories(dir);
  std::string error;
  REQUIRE(write_text_file(dir / "notes.txt", "mine", error));
  CHECK_FALSE(create_game(dir, "empty", error));
  CHECK(error.find("not empty") != std::string::npos);
  CHECK_FALSE(create_game(fresh("nope"), "racing", error));
  std::filesystem::remove_all(dir);
}

TEST_CASE("game.json settings round trip with defaults") {
  constexpr int kWidth = 800;
  constexpr int kHeight = 600;
  const auto dir = fresh("settings");
  std::filesystem::create_directories(dir);
  GameSettings defaults = read_game_settings(dir);
  CHECK(defaults.title == dir.filename().string());
  CHECK(defaults.window_width == config::kWindowWidth);
  std::string error;
  REQUIRE(write_game_settings(dir, {"My Game", kWidth, kHeight}, error));
  const GameSettings saved = read_game_settings(dir);
  CHECK(saved.title == "My Game");
  CHECK(saved.window_width == kWidth);
  CHECK(saved.window_height == kHeight);
  std::filesystem::remove_all(dir);
}

TEST_CASE("building a game makes a standalone folder") {
  const auto game = fresh("to_build");
  const auto out = fresh("built");
  std::string error;
  REQUIRE(create_game(game, "empty", error));
  // Any file stands in for the engine binary here.
  const std::filesystem::path fake_engine = fresh("engine_binary");
  REQUIRE(write_text_file(fake_engine, "binary", error));
  REQUIRE_MESSAGE(build_game(game, out, fake_engine, error), error);
  // The executable is named after the game and plays the game that sits next to it.
  const auto executable = out / executable_name(game);
  REQUIRE(std::filesystem::exists(executable));
  CHECK((std::filesystem::status(executable).permissions() & std::filesystem::perms::owner_exec) !=
        std::filesystem::perms::none);
  CHECK(std::filesystem::exists(out / "game" / config::kSceneFileName));
  CHECK(bundled_game(executable) == out / "game");
  CHECK_FALSE(bundled_game(fake_engine).has_value());  // an executable with no game beside it
  CHECK_FALSE(build_game(fresh("not_a_game"), out, fake_engine, error));
  CHECK(find_games(game.parent_path()).size() >= 1);
  std::filesystem::remove_all(game);
  std::filesystem::remove_all(out);
  std::filesystem::remove(fake_engine);
}

TEST_CASE("a game's executable is named after its folder") {
  CHECK(executable_name("games/platformer") == "platformer");
  CHECK(executable_name("/home/someone/My Game/") == "my_game");
  CHECK(executable_name("games/Super-Kart 64") == "super_kart_64");
  CHECK(executable_name("somewhere/game") == "play");  // "game" is the folder of the game's files
}

TEST_CASE("recently opened games are remembered, newest first, and missing ones forgotten") {
  const std::filesystem::path root = fresh("recent");
  const std::filesystem::path file = root / "config" / "recent.json";
  std::string error;
  REQUIRE(create_game(root / "first", "empty", error));
  REQUIRE(create_game(root / "second", "empty", error));
  CHECK(read_recent_games(file).empty());
  remember_game(file, root / "first");
  remember_game(file, root / "second");
  remember_game(file, root / "first");  // moves back to the top, no duplicate
  const auto recent = read_recent_games(file);
  REQUIRE(recent.size() == 2);
  CHECK(recent[0].filename() == "first");
  CHECK(recent[1].filename() == "second");
  std::filesystem::remove_all(root / "second");
  CHECK(read_recent_games(file).size() == 1);
  CHECK(recent_games_file().filename() == "recent.json");
  CHECK(default_games_folder().filename() == "RedGemGames");
  std::filesystem::remove_all(root);
}

TEST_CASE("game names must make plain folder names, and every template is described") {
  CHECK(valid_game_name("My Game"));
  CHECK(valid_game_name("space-race_2"));
  CHECK_FALSE(valid_game_name(""));
  CHECK_FALSE(valid_game_name(" leading"));
  CHECK_FALSE(valid_game_name("../escape"));
  CHECK_FALSE(valid_game_name("a/b"));
  CHECK_FALSE(valid_game_name(std::string(100, 'a')));
  for (const std::string& name : game_templates()) CHECK_FALSE(template_description(name).empty());
}

TEST_CASE("a game is never built into itself") {
  const std::filesystem::path root = fresh("nested_build");
  const std::filesystem::path game = root / "game";
  std::string error;
  REQUIRE(create_game(game, "empty", error));
  const std::filesystem::path engine = root / "engine_binary";
  REQUIRE(write_text_file(engine, "not really an engine", error));
  // With a trailing slash the name used to be lost and the output landed inside the game.
  CHECK(normalized_folder(game.string() + "/").filename() == "game");
  CHECK_FALSE(build_game(game, game / "out", engine, error));
  CHECK_FALSE(error.empty());
  CHECK_FALSE(build_game(game.string() + "/", game, engine, error));
  CHECK_FALSE(std::filesystem::exists(game / "out"));
  REQUIRE_MESSAGE(build_game(game.string() + "/", root / "built", engine, error), error);
  CHECK(std::filesystem::exists(root / "built" / "game" / "scene.glb"));
  std::filesystem::remove_all(root);
}

TEST_CASE("the platformer template comes with the sounds its scripts play") {
  const std::filesystem::path game = fresh("template_sounds");
  std::string error;
  REQUIRE_MESSAGE(create_game(game, "platformer", error), error);
  for (const char* sound : {"jump", "coin"}) {
    const auto file = find_sound_file(game / config::kSoundsDirName, sound);
    REQUIRE_MESSAGE(file.has_value(), sound);
    CHECK(std::filesystem::file_size(*file) > kMinToneBytes);  // a real tone, not an empty header
  }
  auto engine = play_headless(game);
  for (int i = 0; i < kWalkFrames; ++i) engine->frame(config::kFixedDeltaSeconds);
  std::filesystem::remove_all(game);
}
