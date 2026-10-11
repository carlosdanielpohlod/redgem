#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "app/android_package.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

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
  const std::map<std::string, std::filesystem::path> runtimes = {{"x86_64", scratch.file("built/libredgem.so", "engine")}};
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
  REQUIRE_MESSAGE(stage_android_game(game, runtimes, "stamp-2", stage, error), error);
  const std::string tidy = read(stage / "assets/game.files");
  CHECK(tidy == listing);
  CHECK_FALSE(std::filesystem::exists(stage / "assets/game/.gitignore"));
  CHECK_FALSE(std::filesystem::exists(stage / "assets/game/.git"));
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
