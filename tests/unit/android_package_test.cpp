#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "app/android_package.hpp"
#include "core/platform.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr std::size_t kTooLongPackagePart = 150;  // with "com." before it, past what an id may be
constexpr std::size_t kIdDigits = 6;              // hex digits that tell alike names apart

// A folder of the test's own, removed when the test ends.
struct Scratch {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-android-package-test";
  Scratch() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
  }
  ~Scratch() { std::filesystem::remove_all(dir); }

  std::filesystem::path file(const std::filesystem::path& relative, const std::string& text = "x") const {
    std::filesystem::create_directories((dir / relative).parent_path());
    std::ofstream(dir / relative) << text;
    return dir / relative;
  }
};

std::string read(const std::filesystem::path& file) {
  std::ifstream in(file);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

}  // namespace

TEST_CASE("an app's id is made of the game's letters and digits") {
  CHECK(android_package_name("games/platformer") == "com.redgem.platformer");
  CHECK(android_package_name("games/maze2/") == "com.redgem.maze2");
  CHECK(android_package_name("games/3dmaze") == "com.redgem.game3dmaze");  // a letter first
  // Names that would come out the same get a few digits of their own, so they stay different apps.
  const std::string spaced = android_package_name("/home/someone/My Game/");
  const std::string dashed = android_package_name("games/my-game");
  CHECK(spaced.rfind("com.redgem.mygame_", 0) == 0);
  CHECK(dashed.rfind("com.redgem.mygame_", 0) == 0);
  CHECK(spaced != dashed);
  CHECK(spaced != android_package_name("games/mygame"));
  CHECK(spaced == android_package_name("elsewhere/My Game"));  // the same game, wherever it is
  CHECK(android_package_name("games/---").rfind("com.redgem.game_", 0) == 0);
}

TEST_CASE("the manifest describes a full-screen native activity running the engine") {
  const std::string manifest = android_manifest("com.redgem.mygame", "Tom & Jerry <3>");
  CHECK(manifest.find("package=\"com.redgem.mygame\"") != std::string::npos);
  CHECK(manifest.find("android:label=\"Tom &amp; Jerry &lt;3&gt;\"") != std::string::npos);
  CHECK(manifest.find("android.app.NativeActivity") != std::string::npos);
  CHECK(manifest.find("android:name=\"android.app.lib_name\" android:value=\"redgem\"") != std::string::npos);
  CHECK(manifest.find("android.permission.INTERNET") != std::string::npos);  // multiplayer
  CHECK(manifest.find("glEsVersion=\"0x00030000\"") != std::string::npos);
}

TEST_CASE("the SDK's tools are found in its newest build-tools and platform, or what is missing is said") {
  const Scratch scratch;
  std::string error;
  CHECK_FALSE(find_android_tools({}, error).has_value());
  CHECK(error.find("ANDROID_HOME") != std::string::npos);

  for (const char* tool : {"aapt", "aapt2", "zipalign", "apksigner"}) {
    scratch.file(std::filesystem::path("build-tools/34.0.0") / tool);
    scratch.file(std::filesystem::path("build-tools/35.0.0") / tool);
  }
  CHECK_FALSE(find_android_tools(scratch.dir, error).has_value());  // no platform yet
  CHECK(error.find("android.jar") != std::string::npos);
  scratch.file("platforms/android-9/android.jar");  // older, though "9" sorts after "35" as text
  scratch.file("platforms/android-35/android.jar");
  const auto tools = find_android_tools(scratch.dir, error);
  REQUIRE(tools.has_value());
  CHECK(tools->aapt2 == scratch.dir / "build-tools/35.0.0/aapt2");
  CHECK(tools->platform == scratch.dir / "platforms/android-35/android.jar");
}

TEST_CASE("the engines built for Android are found by kind of processor") {
  const Scratch scratch;
  CHECK(android_runtimes(scratch.dir / "nowhere").empty());
  scratch.file("arm64-v8a/libredgem.so");
  scratch.file("x86_64/libredgem.so");
  scratch.file("notes/readme.txt");
  const auto runtimes = android_runtimes(scratch.dir);
  REQUIRE(runtimes.size() == 2);
  CHECK(runtimes.at("arm64-v8a") == scratch.dir / "arm64-v8a/libredgem.so");
}

TEST_CASE("a game is laid out as an APK's contents: its files, their list, a stamp, the engine, the manifest") {
  const Scratch scratch;
  const std::map<std::string, std::filesystem::path> runtimes = {
      {"x86_64", scratch.file("built/libredgem.so", "engine")}};
  const std::filesystem::path stage = scratch.dir / "stage";
  std::string error;
  REQUIRE_MESSAGE(stage_android_game(fixture("games/sandbox"), runtimes, "stamp-1", stage, error), error);

  CHECK(std::filesystem::exists(stage / "assets/game/scene.glb"));
  CHECK(read(stage / "lib/x86_64/libredgem.so") == "engine");
  CHECK(read(stage / "assets/game.stamp") == "stamp-1");
  CHECK(read(stage / "AndroidManifest.xml").find("com.redgem.sandbox") != std::string::npos);
  // Every file of the game is listed, by its path inside the APK's assets.
  const std::string listing = read(stage / "assets/game.files");
  CHECK(listing.find("game/scene.glb\n") != std::string::npos);
  CHECK(listing.find("game/scenes/level2.glb\n") != std::string::npos);
  std::size_t listed = 0;
  for (const char c : listing) listed += c == '\n' ? 1 : 0;
  std::size_t copied = 0;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(stage / "assets/game")) {
    copied += entry.is_regular_file() ? 1 : 0;
  }
  CHECK(listed == copied);

  // Hidden files and editors' backups are not part of a game.
  const std::filesystem::path game = scratch.dir / "My Game";
  std::filesystem::copy(fixture("games/sandbox"), game, std::filesystem::copy_options::recursive);
  std::ofstream(game / ".gitignore") << "x";
  std::filesystem::create_directories(game / ".git");
  std::ofstream(game / ".git" / "config") << "x";
  std::ofstream(game / "scripts" / "probe.rb~") << "x";
  std::ofstream(game / "release.keystore") << "the developer's signing key";  // never leaves their machine
  std::ofstream(game / "upload.jks") << "another";
  REQUIRE_MESSAGE(stage_android_game(game, runtimes, "stamp-2", stage, error), error);
  const std::string tidy = read(stage / "assets/game.files");
  CHECK(tidy == listing);
  CHECK_FALSE(std::filesystem::exists(stage / "assets/game/.gitignore"));
  CHECK_FALSE(std::filesystem::exists(stage / "assets/game/.git"));
  CHECK_FALSE(std::filesystem::exists(stage / "assets/game/release.keystore"));
  CHECK_FALSE(std::filesystem::exists(stage / "assets/game/upload.jks"));
  // A folder the SDK would silently leave out is refused, by name.
  std::filesystem::create_directories(game / "_shared");
  std::ofstream(game / "_shared" / "data.txt") << "x";
  CHECK_FALSE(stage_android_game(game, runtimes, "s", stage, error));
  CHECK(error.find("_shared/data.txt") != std::string::npos);

  // What cannot be packed is refused with a reason.
  CHECK_FALSE(stage_android_game(scratch.dir, runtimes, "s", stage, error));
  CHECK(error.find("is not a game") != std::string::npos);
  CHECK_FALSE(stage_android_game(fixture("games/sandbox"), {}, "s", stage, error));
  CHECK(error.find("build_runtime.sh") != std::string::npos);
}

TEST_CASE("a game says its own app id, version and signing key in game.json") {
  const Scratch scratch;
  AndroidSettings settings;
  std::string error;
  // Nothing said: nothing to use.
  REQUIRE(read_android_settings(scratch.dir, settings, error));
  CHECK(settings.package.empty());
  CHECK(settings.version_code == 1);
  CHECK(settings.keystore.empty());

  scratch.file("game.json", R"({"title": "Mine", "android": {"package": "com.mystudio.my_game", "version_code": 7,
                                "version_name": "1.4", "keystore": "keys/release.keystore", "key_alias": "mine"}})");
  REQUIRE_MESSAGE(read_android_settings(scratch.dir, settings, error), error);
  CHECK(settings.package == "com.mystudio.my_game");
  CHECK(settings.version_code == 7);
  CHECK(settings.version_name == "1.4");
  CHECK(settings.keystore == "keys/release.keystore");
  CHECK(settings.key_alias == "mine");
  const std::string manifest = android_manifest(settings.package, "Mine", settings);
  CHECK(manifest.find("package=\"com.mystudio.my_game\"") != std::string::npos);
  CHECK(manifest.find("android:versionCode=\"7\" android:versionName=\"1.4\"") != std::string::npos);

  // What cannot be used is refused, saying what is wrong.
  for (const char* bad : {R"({"android": {"package": "MyGame"}})", R"({"android": {"package": "com..game"}})",
                          R"({"android": {"package": "com.9lives"}})", R"({"android": {"version_code": 0}})",
                          R"({"android": {"version_code": "three"}})", R"({"android": {"keystore": "k.keystore"}})",
                          R"({"android": "yes"})"}) {
    CAPTURE(bad);
    scratch.file("game.json", bad);
    CHECK_FALSE(read_android_settings(scratch.dir, settings, error));
    CHECK(error.find("game.json") != std::string::npos);
  }
}

TEST_CASE("a game is signed with its developer's key when it names one, else with this machine's") {
  const Scratch scratch;
  const std::filesystem::path engine_key = scratch.dir / "engine.keystore";
  std::string error;
  AndroidSettings plain;
  const auto engines = signing_key_for(scratch.dir, plain, engine_key, error);
  REQUIRE(engines.has_value());
  CHECK(engines->keystore == engine_key);  // (made on first use)

  AndroidSettings own;
  own.keystore = "keys/release.keystore";
  own.key_alias = "mine";
  platform::set_environment(kKeystorePasswordVariable, "", true);
  CHECK_FALSE(signing_key_for(scratch.dir, own, engine_key, error).has_value());
  CHECK(error.find("does not exist") != std::string::npos);
  scratch.file("keys/release.keystore");
  CHECK_FALSE(signing_key_for(scratch.dir, own, engine_key, error).has_value());
  CHECK(error.find(kKeystorePasswordVariable) != std::string::npos);  // where the password goes

  platform::set_environment(kKeystorePasswordVariable, "secret", true);
  platform::set_environment(kKeyPasswordVariable, "", true);
  const auto theirs = signing_key_for(scratch.dir, own, engine_key, error);
  REQUIRE(theirs.has_value());
  CHECK(theirs->keystore == std::filesystem::absolute(scratch.dir / "keys/release.keystore"));
  CHECK(theirs->alias == "mine");
  CHECK(theirs->store_password_variable == kKeystorePasswordVariable);
  CHECK(theirs->key_password_variable == kKeystorePasswordVariable);  // the key's own was not given
  platform::set_environment(kKeyPasswordVariable, "another", true);
  CHECK(signing_key_for(scratch.dir, own, engine_key, error)->key_password_variable == kKeyPasswordVariable);
  platform::set_environment(kKeystorePasswordVariable, "", true);
  platform::set_environment(kKeyPasswordVariable, "", true);
}

TEST_CASE("an app id's parts each start with a small letter, and none is empty") {
  const Scratch scratch;
  AndroidSettings settings;
  std::string error;
  for (const char* good : {"com.game", "a.b", "com.my_studio.game_2", "c0m.g4me"}) {
    CAPTURE(good);
    scratch.file("game.json", std::string(R"({"android": {"package": ")") + good + R"("}})");
    CHECK_MESSAGE(read_android_settings(scratch.dir, settings, error), error);
    CHECK(settings.package == good);
  }
  for (const char* bad :
       {"game", ".com.game", "com.game.", "com.Game", "com._game", "com.my-game", "com.my game", "com.game.2d"}) {
    CAPTURE(bad);
    scratch.file("game.json", std::string(R"({"android": {"package": ")") + bad + R"("}})");
    CHECK_FALSE(read_android_settings(scratch.dir, settings, error));
    CHECK(error.find("android.package") != std::string::npos);
  }
  // An id longer than Android takes, however well made.
  scratch.file("game.json", R"({"android": {"package": "com.)" + std::string(kTooLongPackagePart, 'a') + R"("}})");
  CHECK_FALSE(read_android_settings(scratch.dir, settings, error));
}

TEST_CASE("what a game's android section gets wrong is named") {
  const Scratch scratch;
  AndroidSettings settings;
  std::string error;
  const auto complaint = [&](const char* json) {
    scratch.file("game.json", json);
    CHECK_FALSE(read_android_settings(scratch.dir, settings, error));
    return error;
  };
  CHECK(complaint(R"({"android": []})") == "game.json: android must be a section ({ ... })");
  CHECK(complaint(R"({"android": {"package": 7}})") == "game.json: android.package must be a text");
  CHECK(complaint(R"({"android": {"version_name": 2}})") == "game.json: android.version_name must be a text");
  CHECK(complaint(R"({"android": {"key_alias": true}})") == "game.json: android.key_alias must be a text");
  CHECK(complaint(R"({"android": {"version_code": 1.5}})").find("android.version_code") != std::string::npos);
  CHECK(complaint(R"({"android": {"keystore": "k.jks"}})").find("android.key_alias is needed") != std::string::npos);
  // A file that is not JSON, or says nothing about Android, is a game with nothing to say.
  scratch.file("game.json", "{ not json");
  CHECK(read_android_settings(scratch.dir, settings, error));
  scratch.file("game.json", R"({"title": "Mine"})");
  CHECK(read_android_settings(scratch.dir, settings, error));
  CHECK(settings.version_name == "1.0");
}

TEST_CASE("an app's id starts with a letter and tells apart names that differ only in what it drops") {
  CHECK(android_package_name("games/42") == "com.redgem.game42");
  CHECK(android_package_name("games/Maze") != android_package_name("games/maze"));
  CHECK(android_package_name("games/Maze").rfind("com.redgem.maze_", 0) == 0);
  const std::string id = android_package_name("games/My Game");
  CHECK(id.size() == std::string("com.redgem.mygame_").size() + kIdDigits);
  CHECK(id.find_first_not_of("0123456789abcdef", id.size() - kIdDigits) == std::string::npos);
}

TEST_CASE("an app holds no file a link points at, and no file whose name would break its list of files") {
  const Scratch scratch;
  const std::map<std::string, std::filesystem::path> runtimes = {
      {"x86_64", scratch.file("built/libredgem.so", "engine")}};
  const std::filesystem::path stage = scratch.dir / "stage";
  const std::filesystem::path game = scratch.dir / "game_src";
  std::filesystem::copy(fixture("games/sandbox"), game, std::filesystem::copy_options::recursive);
  scratch.file("outside/passwords.txt", "secret");
  std::filesystem::create_symlink(scratch.dir / "outside" / "passwords.txt", game / "linked.txt");
  std::filesystem::create_directory_symlink(scratch.dir / "outside", game / "linked_folder");
  std::string error;
  REQUIRE_MESSAGE(stage_android_game(game, runtimes, "stamp", stage, error), error);
  CHECK(std::filesystem::exists(stage / "assets/game/scene.glb"));
  CHECK_FALSE(std::filesystem::exists(stage / "assets/game/linked.txt"));
  CHECK_FALSE(std::filesystem::exists(stage / "assets/game/linked_folder"));
  CHECK(read(stage / "assets/game.files").find("linked") == std::string::npos);

  std::ofstream(game / "scripts" / "two\nlines.rb") << "x";
  CHECK_FALSE(stage_android_game(game, runtimes, "stamp", stage, error));
  CHECK(error.find("line break") != std::string::npos);
  CHECK(error.find("scripts") != std::string::npos);
}
