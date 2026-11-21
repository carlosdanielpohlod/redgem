// What `redgem check` says of game.json's "menu" section (project/menu_checks.cpp): a theme key
// it does not know, a wrong value, and a font or picture the game does not have.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "project/game_checks.hpp"
#include "temp_dirs.hpp"

using namespace redgem::tools;
namespace fs = std::filesystem;

namespace {

// A game of only a game.json with this text, in a folder of its own.
fs::path game_with(const std::string& name, const std::string& game_json) {
  const fs::path dir = redgem::test::temp_root() / ("redgem-menu-check-" + name);
  fs::remove_all(dir);
  fs::create_directories(dir);
  std::ofstream(dir / "game.json") << game_json;
  return dir;
}

void write_file(const fs::path& file) {
  fs::create_directories(file.parent_path());
  std::ofstream(file) << "x";
}

bool says(const CheckReport& report, const std::string& part) {
  for (const std::string& error : report.errors) {
    if (error.find(part) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("a game with no menu section has nothing to say of it") {
  CheckReport report;
  check_menu(game_with("none", R"({"title": "Plain"})"), report);
  CHECK(report.errors.empty());
}

TEST_CASE("a theme key or value the menus do not know is an error that says where it is") {
  CheckReport report;
  check_menu(game_with("wrong", R"({"menu": {"theme": {"colors": {"buton": "red"}, "layout": {"align": "up"}},
                                             "colours": {}}})"),
             report);
  CHECK(says(report, "theme.colors.buton is not a theme setting"));
  CHECK(says(report, "theme.layout.align must be one of left, center, right"));
  CHECK(says(report, "colours is not a menu setting"));
}

TEST_CASE("every font and picture a theme names must be in the game") {
  const fs::path game = game_with("files", R"({"menu": {"theme": {"heading_font": "Display"},
                                                       "screens": {"title": {"logo": {"image": "logo"}}}}})");
  CheckReport missing;
  check_menu(game, missing);
  CHECK(says(missing, "fonts/Display"));
  CHECK(says(missing, "images/logo"));
  write_file(game / "fonts" / "Display.ttf");
  write_file(game / "images" / "logo.png");
  CheckReport there;
  check_menu(game, there);
  CHECK(there.errors.empty());
}
