// Where a game keeps its save, settings, bindings and crash reports: a folder named after its
// studio and title, its players' apart from its developer's.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "app/game_data_location.hpp"
#include "app/game_settings.hpp"
#include "core/platform.hpp"

using namespace redgem;

namespace {

constexpr const char* kStudio = "Pine Hill";
constexpr const char* kTitle = "Coin Rush";
constexpr std::size_t kTwoFolders = 2;

// A game folder of the test's own, with a game.json saying `json`, removed when the test ends.
struct TempGames {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-game-data-location-test";
  TempGames() { std::filesystem::remove_all(root); }
  ~TempGames() { std::filesystem::remove_all(root); }
  TempGames(const TempGames&) = delete;
  TempGames& operator=(const TempGames&) = delete;

  std::filesystem::path game(const std::filesystem::path& folder, const std::string& json) const {
    const std::filesystem::path dir = root / folder;
    std::filesystem::create_directories(dir);
    std::ofstream(dir / kGameSettingsFile) << json;
    return dir;
  }
};

GameSettings named(const std::string& studio, const std::string& title) {
  GameSettings settings;
  settings.studio = studio;
  settings.title = title;
  return settings;
}

std::size_t folders_in(const std::filesystem::path& path) {
  return static_cast<std::size_t>(std::distance(path.begin(), path.end()));
}

}  // namespace

TEST_CASE("a game keeps its save, settings, bindings and crash reports together in its folder") {
  const std::filesystem::path folder = "data";
  const GameDataFiles files = game_data_files(folder);
  CHECK(files.save == folder / "save.json");
  CHECK(files.settings == folder / "settings.json");
  CHECK(files.bindings == folder / "bindings.json");
  CHECK(files.crashes == folder / "crashes");
}

TEST_CASE("a game's folder is named after its studio, then its title") {
  CHECK(game_data_subfolder(named(kStudio, kTitle)) == std::filesystem::path(kStudio) / kTitle);
}

TEST_CASE("a game that names no studio, or has a title of nothing usable, is kept under names of the engine's") {
  CHECK(game_data_subfolder(named("", kTitle)) == std::filesystem::path(kUnnamedStudioFolder) / kTitle);
  CHECK(game_data_subfolder(named("...", kTitle)) == std::filesystem::path(kUnnamedStudioFolder) / kTitle);
  CHECK(game_data_subfolder(named(kStudio, " . ")) == std::filesystem::path(kStudio) / kUnnamedGameFolder);
}

TEST_CASE("no studio or title leads a game's folder anywhere but two folders down") {
  const std::filesystem::path sub = game_data_subfolder(named("../../..", "../../etc/passwd"));
  CHECK(folders_in(sub) == kTwoFolders);
  CHECK_FALSE(sub.is_absolute());
  for (const std::filesystem::path& part : sub) CHECK(part != "..");
  CHECK(game_data_subfolder(named("C:\\", "/root")) == std::filesystem::path("C__") / "_root");
  CHECK(game_data_subfolder(named("CON", "nul")) == std::filesystem::path("CON_") / "nul_");
}

TEST_CASE("a studio and a title in any language name the folder in their own letters") {
  const std::string studio =
      "Est\xC3\xBA"
      "dio A\xC3\xA7\xC3\xA3o";
  const std::string title = "\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0";
  const std::filesystem::path sub = game_data_subfolder(named(studio, title));
  CHECK(sub == std::filesystem::u8path(studio) / std::filesystem::u8path(title));
  CHECK(sub.generic_u8string() == studio + "/" + title);
}

TEST_CASE("the players' folder is where the system keeps programs' data, and follows the game wherever it goes") {
  const TempGames games;
  const std::string json = std::string(R"({"studio": ")") + kStudio + R"(", "title": ")" + kTitle + R"("})";
  const std::filesystem::path here = games.game("here", json);
  const std::filesystem::path players = players_data_folder(here);
  CHECK(players == platform::programs_data_dir() / kStudio / kTitle);
  // Moved, renamed, built: the same studio and title, the same folder.
  CHECK(players_data_folder(games.game(std::filesystem::path("there") / "renamed", json)) == players);
  CHECK(players_data_folder(games.game(std::filesystem::path("dist") / "CoinRush" / "game", json)) == players);
  // Another title is another game.
  CHECK(players_data_folder(games.game("sequel", R"({"studio": "Pine Hill", "title": "Coin Rush 2"})")) != players);
}

TEST_CASE("a system that hands each app a place of its own keeps the game's data there") {
  const TempGames games;
  const std::filesystem::path game = games.game("app", R"({"studio": "Pine Hill", "title": "Coin Rush"})");
  const std::filesystem::path usual = players_data_folder(game);
  set_players_data_folder("/data/user/0/com.pinehill.coinrush/files");
  CHECK(players_data_folder(game) == "/data/user/0/com.pinehill.coinrush/files");
  set_players_data_folder({});
  CHECK(players_data_folder(game) == usual);
}

TEST_CASE("a game its developer plays keeps its data in the developer's folder, never the players'") {
  const TempGames games;
  const std::filesystem::path game = games.game("mine", R"({"studio": "Pine Hill", "title": "Coin Rush"})");
  CHECK(developer_data_folder(game) == platform::user_data_dir() / "game-data" / kStudio / kTitle);
  CHECK(developer_data_folder(game) != players_data_folder(game));
}

TEST_CASE("platform folders of the desktops: XDG_DATA_HOME or .local/share, APPDATA, Application Support") {
  const std::filesystem::path home = "home";
  const std::filesystem::path named = std::filesystem::temp_directory_path() / "named";  // absolute everywhere
  CHECK(platform::programs_data_dir_on(platform::Desktop::Linux, home, named) == named);
  CHECK(platform::programs_data_dir_on(platform::Desktop::Linux, home, {}) == home / ".local" / "share");
  CHECK(platform::programs_data_dir_on(platform::Desktop::Linux, home, "relative") == home / ".local" / "share");
  CHECK(platform::programs_data_dir_on(platform::Desktop::Windows, home, named) == named);
  CHECK(platform::programs_data_dir_on(platform::Desktop::Windows, home, {}) == home / "AppData" / "Roaming");
  CHECK(platform::programs_data_dir_on(platform::Desktop::Mac, home, named) ==
        home / "Library" / "Application Support");
}

TEST_CASE("a game with no title goes by its folder however its path is written, a built one by the folder around it") {
  const TempGames games;
  const std::string json = std::string(R"({"studio": ")") + kStudio + R"("})";
  const std::filesystem::path city = games.game("city", json);
  const std::filesystem::path sub = game_data_subfolder(city);
  CHECK(sub == std::filesystem::path(kStudio) / "city");
  CHECK(game_data_subfolder(city / "") == sub);
  CHECK(game_data_subfolder(city / ".") == sub);
  CHECK(game_data_subfolder(games.game(std::filesystem::path("dist") / "city" / "game", json)) == sub);
}
