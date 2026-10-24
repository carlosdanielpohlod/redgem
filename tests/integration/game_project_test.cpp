#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "app/engine.hpp"
#include "app/bundled_game.hpp"
#include "app/game_settings.hpp"
#include "project/game_build.hpp"
#include "project/game_templates.hpp"
#include "project/recent_games.hpp"
#include "project/notices.hpp"
#include "audio/audio.hpp"
#include "core/config.hpp"
#include "app/app_config.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "editor/scripts/script_files.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "project/icons/exe_icon.hpp"
#include "project/icons/icon_set.hpp"
#include "core/safe_path.hpp"
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
  options.fixed_step = true;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

// A system whose player is made on another one: Windows, or Linux for a test run on Windows.
BuildTarget another_system() { return platform::is_windows() ? BuildTarget::Linux : BuildTarget::Windows; }

// The whole of a file, byte for byte.
std::string bytes_of(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  std::stringstream bytes;
  bytes << in.rdbuf();
  return bytes.str();
}

// A build for a computer that lays the game out beside its executable: this system's, except on
// a Mac, where a build is an app (see game_shipping_test.cpp).
BuildTarget folder_target() {
  return host_build_target() == BuildTarget::MacOS ? BuildTarget::Linux : host_build_target();
}

}  // namespace

TEST_CASE("the templates are the folders of templates/, each with a word about itself") {
  std::vector<std::string> folders;
  for (const auto& entry : std::filesystem::directory_iterator(REDGEM_TEMPLATES_DIR)) {
    if (entry.is_directory()) folders.push_back(entry.path().filename().string());
  }
  std::sort(folders.begin(), folders.end());
  std::vector<std::string> names;
  for (const GameTemplate& known : game_templates()) {
    names.push_back(known.name);
    CHECK_FALSE(known.description.empty());
    CHECK_FALSE(known.icon.empty());
  }
  CHECK(names == folders);
  CHECK(template_names("|") == "empty|platformer");
  // What a new game starts from when nothing else is asked for is one of them.
  CHECK(std::find(names.begin(), names.end(), kDefaultTemplate) != names.end());
}

// What a template's own game.json says (a game that goes online says so) stays in the new game's.
void check_settings_kept(const std::filesystem::path& source, const std::filesystem::path& dir) {
  if (!std::filesystem::exists(source / kGameSettingsFile)) return;
  const nlohmann::json said = nlohmann::json::parse(bytes_of(source / kGameSettingsFile));
  const nlohmann::json kept = nlohmann::json::parse(bytes_of(dir / kGameSettingsFile));
  for (const auto& [key, value] : said.items()) CHECK(kept.at(key) == value);
}

TEST_CASE("a new game is its template's folder, file for file, and a game.json of its own") {
  for (const GameTemplate& known : game_templates()) {
    CAPTURE(known.name);
    const std::filesystem::path source = std::filesystem::path(REDGEM_TEMPLATES_DIR) / known.name;
    const auto dir = fresh("copy_of_" + known.name);
    std::string error;
    REQUIRE_MESSAGE(create_game(dir, known.name, error), error);
    std::size_t copied = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
      const std::filesystem::path name = entry.path().filename();
      if (!entry.is_regular_file() || name == "template.json" || name == kGameSettingsFile) continue;
      const std::filesystem::path file = std::filesystem::relative(entry.path(), source);
      CAPTURE(file);
      CHECK(bytes_of(dir / file) == bytes_of(entry.path()));
      ++copied;
    }
    CHECK(copied > 1);
    // What a template says about itself is the template's, not the game's.
    CHECK_FALSE(std::filesystem::exists(dir / "template.json"));
    CHECK(std::filesystem::exists(dir / kGameSettingsFile));
    check_settings_kept(source, dir);
    std::filesystem::remove_all(dir);
  }
}

TEST_CASE("every template creates a game that loads and whose scripts compile") {
  for (const GameTemplate& known : game_templates()) {
    const std::string& name = known.name;
    CAPTURE(name);
    const auto dir = fresh("template_" + name);
    std::string error;
    REQUIRE_MESSAGE(create_game(dir, name, error), error);
    const LoadResult loaded = load_gltf_scene((dir / config::kSceneFileName).string());
    REQUIRE_MESSAGE(loaded.ok, loaded.error);
    CHECK(loaded.scene.find_node("Camera") != kNoIndex);
    CHECK(loaded.scene.find_node("Sun") != kNoIndex);
    for (const char* folder : {config::kScriptsDirName, config::kSoundsDirName, config::kScenesDirName,
                               config::kModelsDirName, config::kImagesDirName}) {
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

TEST_CASE("a new game gets an id of its own, made from its name") {
  const std::filesystem::path root = fresh("ids");
  std::string error;
  REQUIRE_MESSAGE(create_game(root / "here" / "Coin Rush", "empty", error), error);
  REQUIRE_MESSAGE(create_game(root / "there" / "Coin Rush", "empty", error), error);
  const GameManifest here = read_game_manifest(root / "here" / "Coin Rush");
  const GameManifest there = read_game_manifest(root / "there" / "Coin Rush");
  CHECK(here.id_error.empty());
  CHECK(valid_game_id(here.settings.id));
  CHECK(here.settings.id.rfind("coin-rush-", 0) == 0);
  // Two games given the same name are still two games.
  CHECK(here.settings.id != there.settings.id);
  CHECK(valid_game_id(new_game_id("---")));
  CHECK(new_game_id("---").rfind("game-", 0) == 0);
  CHECK(new_game_id(" My  Game_2 ").rfind("my-game-2-", 0) == 0);
  CHECK(valid_game_id(new_game_id(std::string(64, 'A'))));
  std::filesystem::remove_all(root);
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
  const RubyResult title = engine->scripts().eval("GameMenu.instance.screens[:title].heading", "title.rb");
  REQUIRE_MESSAGE(title.ok, title.error);
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
  CHECK(error == "unknown template 'racing': the templates are empty, platformer");
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
  defaults.title = "My Game";
  defaults.window_width = kWidth;
  defaults.window_height = kHeight;
  REQUIRE(write_game_settings(dir, defaults, error));
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
  // Any file stands in for the player here.
  const std::filesystem::path fake_engine = fresh("engine_binary");
  REQUIRE(write_text_file(fake_engine, "binary", error));
  REQUIRE_MESSAGE(build_game(game, out, fake_engine, BuildTarget::Linux, error), error);
  // The executable is named after the game and plays the game that sits next to it.
  const auto executable = out / executable_name(game);
  REQUIRE(std::filesystem::exists(executable));
  CHECK((std::filesystem::status(executable).permissions() & std::filesystem::perms::owner_exec) !=
        std::filesystem::perms::none);
  CHECK(std::filesystem::exists(out / "game" / config::kSceneFileName));
  CHECK(bundled_game(executable) == out / "game");
  CHECK_FALSE(bundled_game(fake_engine).has_value());  // an executable with no game beside it
  CHECK_FALSE(build_game(fresh("not_a_game"), out, fake_engine, BuildTarget::Linux, error));

  // For Windows the executable is an .exe, made from the Windows player kept beside the editor.
  const auto windows_out = fresh("built_windows");
  REQUIRE_MESSAGE(build_game(game, windows_out, fake_engine, BuildTarget::Windows, error), error);
  CHECK(std::filesystem::exists(windows_out / (executable_name(game) + ".exe")));
  CHECK(std::filesystem::exists(windows_out / "game" / config::kSceneFileName));
  CHECK(build_target_from_name("windows") == BuildTarget::Windows);
  CHECK_FALSE(build_target_from_name("amiga").has_value());
  std::filesystem::remove_all(windows_out);
  CHECK(find_games(game.parent_path()).size() >= 1);
  std::filesystem::remove_all(game);
  std::filesystem::remove_all(out);
  std::filesystem::remove(fake_engine);
}

TEST_CASE("a build ships the player kept beside the program that builds, never that program") {
  const std::filesystem::path install = fresh("install") / "bin";
  const std::filesystem::path editor = install / platform::executable_file_name("redgem", platform::is_windows());
  const std::filesystem::path own_player =
      install / platform::executable_file_name("redgem-player", platform::is_windows());
  const BuildTarget other = another_system();
  const std::filesystem::path other_player =
      install / "runtimes" / build_target_name(other) /
      platform::executable_file_name("redgem-player", other == BuildTarget::Windows);
  std::filesystem::create_directories(other_player.parent_path());
  std::string error;
  REQUIRE(write_text_file(editor, "editor", error));
  // Where each is looked for: the same in a build folder and in an installation.
  CHECK(runtime_location(host_build_target(), editor) == own_player);
  CHECK(runtime_location(other, editor) == other_player);
  CHECK(runtime_location(BuildTarget::Android, editor) == install / "runtimes" / "android");
  // Nothing there yet: no build, and above all not one made of the editor itself.
  CHECK_FALSE(runtime_for(host_build_target(), editor).has_value());
  CHECK_FALSE(runtime_for(other, editor).has_value());
  REQUIRE(write_text_file(own_player, "player", error));
  REQUIRE(write_text_file(other_player, "other player", error));
  CHECK(runtime_for(host_build_target(), editor) == own_player);
  CHECK(runtime_for(other, editor) == other_player);
  CHECK_FALSE(runtime_for(BuildTarget::Android, editor).has_value());  // (packed another way)
  std::filesystem::remove_all(install.parent_path());
}

TEST_CASE("a build with no player to ship says which file is missing and how it is made") {
  const std::filesystem::path missing = "/opt/redgem/bin/runtimes/windows/redgem-player.exe";
  const std::string windows = missing_runtime_message(BuildTarget::Windows, missing);
  CHECK(windows.find("cannot build for windows") != std::string::npos);
  CHECK(windows.find(missing.string()) != std::string::npos);
  CHECK(windows.find("comes with the editor") != std::string::npos);
  // Made by a script on another system; on Windows itself, by the engine's own build.
  const bool built_here = host_build_target() == BuildTarget::Windows;
  CHECK((windows.find("tools/windows/build_runtime.sh") != std::string::npos) != built_here);
  const std::string own = missing_runtime_message(host_build_target(), "/opt/redgem/bin/redgem-player");
  CHECK(own.find("/opt/redgem/bin/redgem-player") != std::string::npos);
  CHECK(own.find("build_runtime.sh") == std::string::npos);  // nothing to cross-build
  CHECK(own.find("redgem-player target") != std::string::npos);
  // This test program has no player beside it: the build it is asked for says so.
  std::string error;
  const auto built =
      build_game_for(redgem::test::fixture("games/sandbox"), fresh("no_player"), host_build_target(), error);
  CHECK_FALSE(built.has_value());
  CHECK(error.find(runtime_location(host_build_target(), platform::executable_path()).string()) != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(fresh("no_player")));
}

TEST_CASE("the game built from the sandbox is the real player under the game's name, beside its files") {
  const std::filesystem::path player = REDGEM_PLAYER_FILE;
  const std::filesystem::path editor = REDGEM_EDITOR_FILE;
  REQUIRE(std::filesystem::is_regular_file(player));
  // The build finds it where the editor of this build would: beside redgem.
  REQUIRE(runtime_for(host_build_target(), editor) == player);
  const std::filesystem::path out = fresh("built_sandbox");
  std::string error;
  const std::filesystem::path game = redgem::test::fixture("games/sandbox");
  REQUIRE_MESSAGE(build_game(game, out, *runtime_for(host_build_target(), editor), folder_target(), error), error);
  const std::filesystem::path executable =
      out / platform::executable_file_name(executable_name(game), platform::is_windows());
  REQUIRE(std::filesystem::is_regular_file(executable));
  CHECK(executable.stem() == "sandbox");
  CHECK(std::filesystem::file_size(executable) == std::filesystem::file_size(player));
  CHECK(bytes_of(executable) == bytes_of(player));
  // Not the program games are made with, which is another file altogether.
  CHECK(std::filesystem::file_size(executable) < std::filesystem::file_size(editor));
  CHECK(bytes_of(executable).find("Dear ImGui") == std::string::npos);
  CHECK(bytes_of(editor).find("Dear ImGui") != std::string::npos);
  CHECK(bundled_game(executable) == out / "game");
  for (const std::filesystem::path& file : shipped_files(game)) {
    CAPTURE(file);
    CHECK(bytes_of(out / "game" / file) == bytes_of(game / file));
  }
  std::filesystem::remove_all(out);
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
  CHECK(recent_games_file() == platform::user_data_dir() / "recent.json");
  CHECK(default_games_folder().filename() == "RedGemGames");
  std::filesystem::remove_all(root);
}

TEST_CASE("game names must make plain folder names") {
  CHECK(valid_game_name("My Game"));
  CHECK(valid_game_name("space-race_2"));
  CHECK_FALSE(valid_game_name(""));
  CHECK_FALSE(valid_game_name(" leading"));
  CHECK_FALSE(valid_game_name("../escape"));
  CHECK_FALSE(valid_game_name("a/b"));
  CHECK_FALSE(valid_game_name(std::string(100, 'a')));
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
  CHECK_FALSE(build_game(game, game / "out", engine, BuildTarget::Linux, error));
  CHECK_FALSE(error.empty());
  CHECK_FALSE(build_game(game.string() + "/", game, engine, BuildTarget::Linux, error));
  CHECK_FALSE(std::filesystem::exists(game / "out"));
  REQUIRE_MESSAGE(build_game(game.string() + "/", root / "built", engine, BuildTarget::Linux, error), error);
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

TEST_CASE("saving a game's settings keeps what else its game.json says") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-settings-keep-test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  std::ofstream(dir / "game.json")
      << R"({"title": "Old", "android": {"package": "com.mystudio.mine"}, "notes": [1, 2]})";
  GameSettings settings = read_game_settings(dir);
  settings.title = "New";
  std::string error;
  REQUIRE_MESSAGE(write_game_settings(dir, settings, error), error);
  const nlohmann::json saved = nlohmann::json::parse(bytes_of(dir / "game.json"));
  CHECK(saved["title"] == "New");
  CHECK(saved["android"]["package"] == "com.mystudio.mine");
  CHECK(saved["notes"].size() == 2);
  std::filesystem::remove_all(dir);
}

TEST_CASE("a built game never carries a signing key left in its folder") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-build-keys-test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  std::filesystem::copy(redgem::test::fixture("games/sandbox"), dir / "game_src",
                        std::filesystem::copy_options::recursive);
  std::ofstream(dir / "game_src" / "release.keystore") << "secret";
  std::ofstream(dir / "engine") << "engine";
  std::string error;
  REQUIRE_MESSAGE(build_game(dir / "game_src", dir / "out", dir / "engine", folder_target(), error), error);
  CHECK(std::filesystem::exists(dir / "out" / "game" / "scene.glb"));
  CHECK_FALSE(std::filesystem::exists(dir / "out" / "game" / "release.keystore"));
  CHECK(signing_key_file("keys/upload.jks"));
  CHECK(signing_key_file("release.p12"));
  CHECK_FALSE(signing_key_file("scene.glb"));
  std::filesystem::remove_all(dir);
}

// Whatever its case or kind, and whatever the game.json's keystore is called.
TEST_CASE("a built game carries no key in any case, nor the keystore its game.json names") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-build-named-keys-test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::filesystem::path game = dir / "game_src";
  std::filesystem::copy(redgem::test::fixture("games/sandbox"), game, std::filesystem::copy_options::recursive);
  const std::vector<std::string> keys = {"Release.KEYSTORE", "upload.JKS", "cert.pem", "store.pfx", "signing.key"};
  for (const std::string& key : keys) std::ofstream(game / key) << "secret";
  std::filesystem::create_directories(game / "secrets");
  std::ofstream(game / "secrets" / "mine.bin") << "secret";
  std::ofstream(game / "game.json") << R"({"android": {"keystore": "secrets/../secrets/mine.bin"}})";
  std::ofstream(dir / "engine") << "engine";
  std::string error;
  REQUIRE_MESSAGE(build_game(game, dir / "out", dir / "engine", folder_target(), error), error);
  for (const std::string& key : keys) {
    CAPTURE(key);
    CHECK_FALSE(std::filesystem::exists(dir / "out" / "game" / key));
  }
  CHECK_FALSE(std::filesystem::exists(dir / "out" / "game" / "secrets" / "mine.bin"));
  CHECK(std::filesystem::exists(dir / "out" / "game" / "game.json"));
  std::filesystem::remove_all(dir);
}

// The keystore is told by being the same file: an absolute path, or one in another case where the
// system ignores case (Windows, macOS), names it as well.
TEST_CASE("a built game never carries the keystore its game.json names by an absolute path or in another case") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-build-keystore-path-test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::filesystem::path game = dir / "game_src";
  std::filesystem::copy(redgem::test::fixture("games/sandbox"), game, std::filesystem::copy_options::recursive);
  std::ofstream(game / "upload.bin") << "secret";
  std::ofstream(game / "release.bin") << "secret";
  std::ofstream(dir / "engine") << "engine";
  const nlohmann::json named = {{"android", {{"keystore", (game / "upload.bin").string()}}}};
  std::ofstream(game / "game.json") << named.dump();
  std::string error;
  REQUIRE_MESSAGE(build_game(game, dir / "out", dir / "engine", folder_target(), error), error);
  CHECK_FALSE(std::filesystem::exists(dir / "out" / "game" / "upload.bin"));

  const bool case_ignored = std::filesystem::exists(game / "RELEASE.BIN");
  std::ofstream(game / "game.json") << R"({"android": {"keystore": "RELEASE.BIN"}})";
  REQUIRE_MESSAGE(build_game(game, dir / "out", dir / "engine", folder_target(), error), error);
  CHECK(std::filesystem::exists(dir / "out" / "game" / "release.bin") == !case_ignored);
  std::filesystem::remove_all(dir);
}

TEST_CASE("a game.json that holds the wrong kinds of things is read as far as it makes sense") {
  constexpr int kHuge = 2000000000;
  const auto dir = fresh("odd_settings");
  std::filesystem::create_directories(dir);
  const auto settings_of = [&dir](const std::string& text) {
    std::ofstream(dir / "game.json") << text;
    return read_game_settings(dir);
  };
  const GameSettings wrong = settings_of(R"({"title": 5, "window_width": "wide", "window_height": [720]})");
  CHECK(wrong.title == dir.filename().string());
  CHECK(wrong.window_width == config::kWindowWidth);
  CHECK(wrong.window_height == config::kWindowHeight);
  const GameSettings fractional = settings_of(R"({"title": {"a": 1}, "window_width": 800.5, "window_height": null})");
  CHECK(fractional.window_width == config::kWindowWidth);
  CHECK(fractional.window_height == config::kWindowHeight);

  // A window is neither a speck nor larger than any screen.
  const GameSettings tiny = settings_of(R"({"window_width": 0, "window_height": -600})");
  CHECK(tiny.window_width == config::kMinWindowSide);
  CHECK(tiny.window_height == config::kMinWindowSide);
  const GameSettings vast =
      settings_of(R"({"window_width": )" + std::to_string(kHuge) + R"(, "window_height": 99999999999999999999})");
  CHECK(vast.window_width == config::kMaxWindowSide);
  CHECK(vast.window_height >= config::kMinWindowSide);
  CHECK(vast.window_height <= config::kMaxWindowSide);

  CHECK(settings_of("{\"title\": \"Line\\nbreak\\u001b[2J\"}").title == "Line?break?[2J");
  CHECK(settings_of("[1, 2]").title == dir.filename().string());
  std::filesystem::remove_all(dir);
}

TEST_CASE("a built game holds the game's own files: nothing hidden, no backups, nothing a link points at") {
  const std::filesystem::path dir = fresh("build_tidy");
  std::filesystem::create_directories(dir / "outside");
  std::filesystem::copy(redgem::test::fixture("games/sandbox"), dir / "game_src",
                        std::filesystem::copy_options::recursive);
  const std::filesystem::path game = dir / "game_src";
  std::ofstream(dir / "outside" / "passwords.txt") << "secret";
  std::ofstream(game / ".env") << "TOKEN=secret";
  std::filesystem::create_directories(game / ".git");
  std::ofstream(game / ".git" / "config") << "x";
  std::ofstream(game / "scripts" / "probe.rb~") << "x";
  std::ofstream(game / "notes.txt") << "kept";
  std::filesystem::create_symlink(dir / "outside" / "passwords.txt", game / "linked.txt");
  std::filesystem::create_directory_symlink(dir / "outside", game / "linked_folder");
  std::ofstream(dir / "engine") << "engine";
  std::string error;
  REQUIRE_MESSAGE(build_game(game, dir / "out", dir / "engine", folder_target(), error), error);
  const std::filesystem::path built = dir / "out" / "game";
  CHECK(std::filesystem::exists(built / "scene.glb"));
  CHECK(std::filesystem::exists(built / "scenes" / "level2.glb"));
  CHECK(std::filesystem::exists(built / "notes.txt"));
  for (const char* left_behind : {".env", ".git", "scripts/probe.rb~", "linked.txt", "linked_folder"}) {
    CAPTURE(left_behind);
    CHECK_FALSE(std::filesystem::exists(std::filesystem::symlink_status(built / left_behind)));
  }
  const std::vector<std::filesystem::path> shipped = shipped_files(game);
  CHECK(std::find(shipped.begin(), shipped.end(), "notes.txt") != shipped.end());
  CHECK(std::find(shipped.begin(), shipped.end(), "linked.txt") == shipped.end());
  CHECK(std::is_sorted(shipped.begin(), shipped.end()));
  std::filesystem::remove_all(dir);
}

TEST_CASE("a built game carries the licenses of what it contains") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-build-notices-test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  std::ofstream(dir / "engine") << "engine";
  std::string error;
  REQUIRE_MESSAGE(
      build_game(redgem::test::fixture("games/sandbox"), dir / "out", dir / "engine", folder_target(), error), error);
  const auto text_of = [&dir](const char* file) {
    std::ifstream in(dir / "out" / kNoticesFolder / file);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
  };
  CHECK(text_of("LICENSE").find("MIT License") != std::string::npos);
  // Every project the engine is built on is named, with its license text.
  const std::string others = text_of("THIRD_PARTY_LICENSES.md");
  for (const char* project :
       {"raylib", "Jolt Physics", "mruby", "cgltf", "nlohmann/json", "Inter", "Font Awesome Free"}) {
    CAPTURE(project);
    CHECK(others.find(project) != std::string::npos);
  }
  std::filesystem::remove_all(dir);
}

#ifdef _WIN32  // (the player built here is the Windows one)
TEST_CASE("the Windows player carries the engine's icon, with room in its headers for a game's") {
  const auto player = read_file_up_to(REDGEM_PLAYER_FILE, std::uintmax_t{1} << 30);
  REQUIRE(player.has_value());
  CHECK(exe_icon_sides(*player) == std::vector<int>(kWindowsIconSides.begin(), kWindowsIconSides.end()));
  CHECK(exe_has_room_for_icon(*player));
}
#endif
