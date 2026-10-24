#include <doctest/doctest.h>

#include <raylib.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/bundled_game.hpp"
#include "app/game_identity.hpp"
#include "app/game_settings.hpp"
#include "app/splash_screen.hpp"
#include "core/config.hpp"
#include "core/image_header.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr ::Color kRed{230, 41, 55, 255};  // (raylib's own colors are macros the engine's Color would clash with)

constexpr int kSplashSide = 8;

// A game folder of the test's own, with the game.json given.
std::filesystem::path game_saying(const std::string& json) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_game_identity_test" / "game";
  std::filesystem::remove_all(dir.parent_path());
  std::filesystem::create_directories(dir);
  std::ofstream(dir / config::kSceneFileName) << "scene";
  if (!json.empty()) std::ofstream(dir / kGameSettingsFile) << json;
  return dir;
}

void write_png(const std::filesystem::path& file, int width, int height) {
  std::filesystem::create_directories(file.parent_path());
  const Image image = GenImageColor(width, height, kRed);
  REQUIRE(ExportImage(image, file.string().c_str()));
  UnloadImage(image);
}

}  // namespace

TEST_CASE("a version is a semantic version: three numbers, then what comes before or after a release") {
  for (const char* version : {"1.2.0", "0.0.1", "10.20.30", "1.0.0-beta", "1.0.0-beta.1", "1.0.0-rc.1+sha.5114f85",
                              "1.0.0+20130313144700", "1.0.0+001", "1.0.0-x-y-z.--"}) {
    CHECK_MESSAGE(valid_game_version(version), version);
  }
  for (const char* version : {"", "1", "1.2", "1.2.3.4", "v1.2.3", "01.2.3", "1.02.3", "1.2.3-", "1.2.3+", "1.2.3-01",
                              "1.2.3-beta..1", "1.2.3-beta_1", "1.2.3 ", "-1.2.3", "1.2.3+a+b"}) {
    CHECK_FALSE_MESSAGE(valid_game_version(version), version);
  }
  CHECK_FALSE(valid_game_version("1.2.3-" + std::string(kMaxVersionLength, 'a')));
}

TEST_CASE("the numbers of a version are what comes before its pre-release and build parts") {
  CHECK(version_numbers("2.0.0") == "2.0.0");
  CHECK(version_numbers("2.0.0-beta.1") == "2.0.0");
  CHECK(version_numbers("2.0.0+build.7") == "2.0.0");
  CHECK(version_numbers("2.0.0-rc.1+build-7") == "2.0.0");
}

TEST_CASE("a game that says nothing of itself has no version, no pictures and nothing about the network") {
  const GameIdentityReading reading = read_game_identity(game_saying(""));
  CHECK(reading.error.empty());
  CHECK(reading.identity.version.empty());
  CHECK(reading.identity.icon.empty());
  CHECK(reading.identity.splash.empty());
  CHECK_FALSE(reading.identity.network.has_value());
}

TEST_CASE("game.json gives a game its version, its icon, its splash picture and the network") {
  const GameIdentityReading reading = read_game_identity(game_saying(
      R"({"version": "1.2.0", "icon": "images/icon.png", "splash": "images/Splash.PNG", "network": true})"));
  CHECK(reading.error.empty());
  CHECK(reading.identity.version == "1.2.0");
  CHECK(reading.identity.icon == std::filesystem::path("images/icon.png"));
  CHECK(reading.identity.splash == std::filesystem::path("images/Splash.PNG"));
  CHECK(reading.identity.network == true);
  CHECK(read_game_identity(game_saying(R"({"network": false})")).identity.network == false);
}

TEST_CASE("what game.json says of the game and cannot be used is left out, and said") {
  const auto complaint = [](const std::string& json) { return read_game_identity(game_saying(json)).error; };
  CHECK(complaint(R"({"version": "1.2"})").find("game.json: version must be a version like 1.2.0") == 0);
  CHECK(complaint(R"({"version": 3})").find("game.json: version") == 0);
  CHECK(complaint(R"({"icon": "../icon.png"})").find("game.json: icon must name a .png file inside") == 0);
  CHECK(complaint(R"({"icon": "/tmp/icon.png"})").find("game.json: icon") == 0);
  CHECK(complaint(R"({"splash": "images/splash.jpg"})").find("game.json: splash") == 0);
  CHECK(complaint(R"({"network": "yes"})") == "game.json: network must be true or false");
  // The first thing wrong is said; the rest that can be used still is.
  const GameIdentityReading reading = read_game_identity(game_saying(R"({"version": "x", "network": true})"));
  CHECK(reading.identity.version.empty());
  CHECK(reading.identity.network == true);
}

TEST_CASE("the splash picture is a PNG inside the game that can be decoded") {
  const std::filesystem::path game = game_saying(R"({"splash": "images/splash.png"})");
  test::LogCapture log;
  CHECK_FALSE(splash_picture(game, read_game_identity(game).identity));  // not there yet
  CHECK(log.contains("the splash picture images/splash.png is not a PNG"));
  std::ofstream(game / "images" / "splash.png") << "not a picture";
  CHECK_FALSE(splash_picture(game, read_game_identity(game).identity));
  write_png(game / "images" / "splash.png", kSplashSide, kSplashSide);
  const auto picture = splash_picture(game, read_game_identity(game).identity);
  REQUIRE(picture);
  CHECK(encoded_image_size(picture->data(), picture->size())->width == kSplashSide);
  CHECK_FALSE(splash_picture(game_saying(""), {}));
}

TEST_CASE("a Mac app plays the game kept in its resources") {
  const std::filesystem::path app = game_saying("").parent_path() / "Game.app" / kMacContentsFolder;
  const std::filesystem::path game = app / kMacResourcesFolder / kBundledGameFolder;
  std::filesystem::create_directories(game);
  std::ofstream(game / config::kSceneFileName) << "scene";
  CHECK(bundled_game(app / kMacProgramFolder / "game") == game);
  // A program elsewhere has no app around it: a Resources folder near it is nobody's game.
  CHECK_FALSE(bundled_game(app / "elsewhere" / "game"));
}
