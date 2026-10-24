#include <doctest/doctest.h>

#include <raylib.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "app/bundled_game.hpp"
#include "app/game_settings.hpp"
#include "core/config.hpp"
#include "core/image_header.hpp"
#include "core/translations.hpp"
#include "project/game_shipping.hpp"
#include "project/icons/bytes.hpp"
#include "project/icons/exe_icon.hpp"
#include "project/icons/icon_set.hpp"
#include "project/notices.hpp"

using namespace redgem;
using bytes::Bytes;
using bytes::read_big;

namespace {

constexpr ::Color kRed{230, 41, 55, 255};
constexpr ::Color kBlue{0, 121, 241, 255};  // (raylib's own colors are macros the engine's Color would clash with)

constexpr int kIconSide = 64;  // the test's icon: small, so it is quick to scale
constexpr std::size_t kIcnsHeaderBytes = 8;

// A folder of the test's own, removed when the test ends, with a game in it.
struct Scratch {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-game-shipping-test";
  std::filesystem::path game = dir / "My Game";
  std::filesystem::path out = dir / "out";
  Scratch() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(game / "images");
    std::ofstream(game / config::kSceneFileName) << "scene";
  }
  ~Scratch() { std::filesystem::remove_all(dir); }

  void say(const std::string& json) const { std::ofstream(game / kGameSettingsFile) << json; }
};

void write_png(const std::filesystem::path& file, int width, int height) {
  const Image image = GenImageGradientLinear(width, height, 0, kRed, kBlue);
  REQUIRE(ExportImage(image, file.string().c_str()));
  UnloadImage(image);
}

Bytes read_bytes(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string read_text(const std::filesystem::path& file) {
  const Bytes data = read_bytes(file);
  return std::string(data.begin(), data.end());
}

// The side of the square PNG `png` is.
int png_side(const Bytes& png) {
  const auto size = encoded_image_size(png.data(), png.size());
  REQUIRE(size);
  CHECK(size->width == size->height);
  return static_cast<int>(size->width);
}

IconSet test_icons(const Scratch& scratch) {
  write_png(scratch.game / "images" / "icon.png", kIconSide, kIconSide);
  std::string error;
  const auto icons = icon_set_from(scratch.game / "images" / "icon.png", kWindowsIconSides, error);
  REQUIRE_MESSAGE(icons, error);
  return *icons;
}

}  // namespace

TEST_CASE("an icon is scaled to each size a system asks for, as square PNGs") {
  const Scratch scratch;
  const IconSet icons = test_icons(scratch);
  REQUIRE(icons.size() == kWindowsIconSides.size());
  for (const int side : kWindowsIconSides) CHECK(png_side(icons.at(side)) == side);
}

TEST_CASE("an icon must be a square PNG that can be read") {
  const Scratch scratch;
  std::string error;
  write_png(scratch.game / "images" / "wide.png", kIconSide * 2, kIconSide);
  CHECK_FALSE(icon_set_from(scratch.game / "images" / "wide.png", kMacIconSides, error));
  CHECK(error.find("is not a square PNG") != std::string::npos);
  std::ofstream(scratch.game / "images" / "broken.png") << "not a picture";
  CHECK_FALSE(icon_set_from(scratch.game / "images" / "broken.png", kMacIconSides, error));
  CHECK_FALSE(icon_set_from(scratch.game / "images" / "missing.png", kMacIconSides, error));
}

TEST_CASE("a Mac icon file holds the sizes Apple reads as PNGs, each after its type and length") {
  const Scratch scratch;
  write_png(scratch.game / "images" / "icon.png", kIconSide, kIconSide);
  std::string error;
  const auto icons = icon_set_from(scratch.game / "images" / "icon.png", kMacIconSides, error);
  REQUIRE(icons);
  const Bytes icns = icns_file(*icons);
  CHECK(std::string(icns.begin(), icns.begin() + 4) == "icns");
  CHECK(read_big(icns, 4, 4) == icns.size());
  std::map<std::string, int> found;
  for (std::size_t at = kIcnsHeaderBytes; at < icns.size(); at += read_big(icns, at + 4, 4)) {
    const Bytes png(icns.begin() + at + kIcnsHeaderBytes, icns.begin() + at + read_big(icns, at + 4, 4));
    found[std::string(icns.begin() + at, icns.begin() + at + 4)] = png_side(png);
  }
  const std::map<std::string, int> expected = {{"icp4", 16},  {"icp5", 32},  {"icp6", 64},  {"ic07", 128},
                                               {"ic08", 256}, {"ic09", 512}, {"ic10", 1024}};
  CHECK(found == expected);
  CHECK(icns_file({{24, Bytes{1, 2}}}).size() == kIcnsHeaderBytes);  // a size Apple has no type for is left out
}

TEST_CASE("a Linux menu entry names the game, its program and its icon") {
  CHECK(desktop_entry("Coin Rush", "coin_rush", true) ==
        "[Desktop Entry]\nType=Application\nName=Coin Rush\nExec=coin_rush\nIcon=coin_rush\n"
        "Categories=Game;\nTerminal=false\n");
  const std::string without_icon = desktop_entry("Back\\slash\nTwo\tTabs", "game", false);
  CHECK(without_icon.find("Name=Back\\\\slash\\nTwo\\tTabs\n") != std::string::npos);
  CHECK(without_icon.find("Icon=") == std::string::npos);
}

TEST_CASE("a Mac app's description says its name, id, program, version and icon") {
  const std::string plist = mac_info_plist({"Tom & Jerry", "tom_jerry", "com.studio.tom-jerry", "1.2.0-beta.1", true});
  CHECK(plist.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist", 0) == 0);
  CHECK(plist.find("<key>CFBundleName</key>\n  <string>Tom &amp; Jerry</string>") != std::string::npos);
  CHECK(plist.find("<key>CFBundleIdentifier</key>\n  <string>com.studio.tom-jerry</string>") != std::string::npos);
  CHECK(plist.find("<key>CFBundleExecutable</key>\n  <string>tom_jerry</string>") != std::string::npos);
  CHECK(plist.find("<key>CFBundlePackageType</key>\n  <string>APPL</string>") != std::string::npos);
  // The Mac takes numbers only.
  CHECK(plist.find("<key>CFBundleShortVersionString</key>\n  <string>1.2.0</string>") != std::string::npos);
  CHECK(plist.find("<key>CFBundleVersion</key>\n  <string>1.2.0</string>") != std::string::npos);
  CHECK(plist.find("<key>CFBundleIconFile</key>\n  <string>tom_jerry.icns</string>") != std::string::npos);
  CHECK(plist.find("</dict>\n</plist>\n") != std::string::npos);
  const std::string plain = mac_info_plist({"Game", "game", "com.redgem.game", "", false});
  CHECK(plain.find("<string>1.0</string>") != std::string::npos);
  CHECK(plain.find("CFBundleIconFile") == std::string::npos);
}

TEST_CASE("a Mac app is named after the game, with an id a bundle may have") {
  CHECK(mac_bundle_id("com.my_studio.my_game", "my_game") == "com.my-studio.my-game");
  CHECK(mac_bundle_id("", "my_game") == "com.redgem.my-game");
  CHECK(mac_app_folder("Coin Rush", "coin_rush") == "Coin Rush.app");
  CHECK(mac_app_folder("A/B: C\\D", "x") == "AB CD.app");
  CHECK(mac_app_folder(" ..Hidden. ", "x") == "Hidden.app");
  CHECK(mac_app_folder("/:", "coin_rush") == "coin_rush.app");
}

TEST_CASE("a build for Linux ships a menu entry and the game's icon beside the executable") {
  const Scratch scratch;
  std::ofstream(scratch.dir / "player") << "player";
  write_png(scratch.game / "images" / "icon.png", kIconSide, kIconSide);
  scratch.say(R"({"title": "My Game", "icon": "images/icon.png"})");
  std::string error;
  REQUIRE_MESSAGE(build_game(scratch.game, scratch.out, scratch.dir / "player", BuildTarget::Linux, error), error);
  CHECK(read_text(scratch.out / "my_game.desktop").find("Icon=my_game\n") != std::string::npos);
  CHECK(png_side(read_bytes(scratch.out / "my_game.png")) == kLinuxIconSide);
  // A later build of the game without an icon takes the earlier build's away.
  scratch.say(R"({"title": "My Game"})");
  REQUIRE_MESSAGE(build_game(scratch.game, scratch.out, scratch.dir / "player", BuildTarget::Linux, error), error);
  CHECK_FALSE(std::filesystem::exists(scratch.out / "my_game.png"));
  CHECK(read_text(scratch.out / "my_game.desktop").find("Icon=") == std::string::npos);
}

TEST_CASE("a build refuses a game whose game.json says what cannot be shipped") {
  const Scratch scratch;
  std::ofstream(scratch.dir / "player") << "player";
  std::string error;
  scratch.say(R"({"icon": "images/icon.png"})");
  CHECK_FALSE(build_game(scratch.game, scratch.out, scratch.dir / "player", BuildTarget::Linux, error));
  CHECK(error == tr("project.icon.missing", {{"picture", "images/icon.png"}}));
  scratch.say(R"({"version": "one"})");
  CHECK_FALSE(build_game(scratch.game, scratch.out, scratch.dir / "player", BuildTarget::Linux, error));
  CHECK(error.find("game.json: version") == 0);
  CHECK_FALSE(std::filesystem::exists(scratch.out / "game"));  // nothing was written
}

TEST_CASE("a build for the Mac is an app with the player, the game, its icon and its description") {
  const Scratch scratch;
  std::ofstream(scratch.dir / "player") << "player";
  write_png(scratch.game / "images" / "icon.png", kIconSide, kIconSide);
  scratch.say(R"({"title": "Coin Rush", "id": "com.studio.coin_rush", "version": "2.1.0", "icon": "images/icon.png"})");
  std::string error;
  REQUIRE_MESSAGE(build_game(scratch.game, scratch.out, scratch.dir / "player", BuildTarget::MacOS, error), error);
  const std::filesystem::path contents = scratch.out / "Coin Rush.app" / kMacContentsFolder;
  CHECK(read_text(contents / kMacProgramFolder / "my_game") == "player");
  CHECK(bundled_game(contents / kMacProgramFolder / "my_game") == contents / kMacResourcesFolder / kBundledGameFolder);
  CHECK(std::filesystem::exists(contents / kMacResourcesFolder / kNoticesFolder));
  CHECK(read_text(contents / kMacResourcesFolder / "my_game.icns").rfind("icns", 0) == 0);
  const std::string plist = read_text(contents / "Info.plist");
  CHECK(plist.find("<string>com.studio.coin-rush</string>") != std::string::npos);
  CHECK(plist.find("<string>2.1.0</string>") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(scratch.out / "my_game"));  // no program beside the app
  // Built again, the app is made afresh: nothing of the last one stays in it.
  std::ofstream(contents / "leftover") << "x";
  REQUIRE_MESSAGE(build_game(scratch.game, scratch.out, scratch.dir / "player", BuildTarget::MacOS, error), error);
  CHECK_FALSE(std::filesystem::exists(contents / "leftover"));
}

TEST_CASE("a Mac app is not written over what no build made") {
  const Scratch scratch;
  std::ofstream(scratch.dir / "player") << "player";
  std::filesystem::create_directories(scratch.out / "My Game.app");
  std::string error;
  CHECK_FALSE(build_game(scratch.game, scratch.out, scratch.dir / "player", BuildTarget::MacOS, error));
  CHECK(error.find("My Game.app") != std::string::npos);
}
