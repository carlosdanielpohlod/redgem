#include "app/bundled_game.hpp"

#include <utility>

#include "app/raylib_file_reading.hpp"
#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/game_archive.hpp"
#include "core/game_files.hpp"

namespace redgem {
namespace {

// Whether a program is the one of a Mac app: <name>.app/Contents/MacOS/<program>.
bool in_mac_app(const std::filesystem::path& executable) {
  const std::filesystem::path folder = executable.parent_path();
  return folder.filename() == kMacProgramFolder && folder.parent_path().filename() == kMacContentsFolder;
}

// Where the game of a program is kept: beside it, or in the resources of the app it is in.
std::filesystem::path game_place(const std::filesystem::path& executable) {
  if (!in_mac_app(executable)) return executable.parent_path();
  return executable.parent_path().parent_path() / kMacResourcesFolder;
}

bool has_archive(const std::filesystem::path& place) {
  std::error_code fs_error;
  return std::filesystem::is_regular_file(place / kArchiveFileName, fs_error);
}

// Whether a place holds a game: packed, or loose in its game folder.
bool holds_a_game(const std::filesystem::path& place) {
  return has_archive(place) || is_game_folder(place / kBundledGameFolder);
}

std::optional<std::filesystem::path> loose_game(const std::filesystem::path& game, std::string& error) {
  if (is_game_folder(game)) return game;
  error = "no game to play: neither " + (game.parent_path() / kArchiveFileName).string() + " nor " +
          (game / config::kSceneFileName).string() + " is there";
  return std::nullopt;
}

// Mounts the archive in `place` as the game folder `game`.
std::optional<std::filesystem::path> packed_game(const std::filesystem::path& place, const std::filesystem::path& game,
                                                 std::string& error) {
  auto archive = GameArchive::open(place / kArchiveFileName, error);
  if (!archive) return std::nullopt;
  game_files::mount(game, std::move(*archive));
  read_raylib_files_through_game_files();
  if (is_game_folder(game)) return game;
  error = "no game to play: " + (place / kArchiveFileName).string() + " holds no " + config::kSceneFileName;
  game_files::unmount();
  return std::nullopt;
}

}  // namespace

std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable) {
  const std::filesystem::path place = game_place(executable);
  if (!holds_a_game(place)) return std::nullopt;
  return place / kBundledGameFolder;
}

std::optional<std::filesystem::path> open_bundled_game(const std::filesystem::path& executable, std::string& error) {
  return open_game_in(game_place(executable), error);
}

std::optional<std::filesystem::path> open_game_in(const std::filesystem::path& place, std::string& error) {
  const std::filesystem::path game = place / kBundledGameFolder;
  // (A build makes one or the other, and takes away what an earlier build made of the other.)
  if (has_archive(place)) return packed_game(place, game, error);
  return loose_game(game, error);
}

}  // namespace redgem
