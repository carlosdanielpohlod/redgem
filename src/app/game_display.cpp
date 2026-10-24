#include "app/game_display.hpp"

#include "app/game_settings.hpp"
#include "app/save_location.hpp"
#include "app/user_settings.hpp"

namespace redgem {

std::filesystem::path player_settings_file(const std::filesystem::path& given) {
  return given.empty() ? user_settings_file() : given;
}

DisplaySettings game_display(const std::filesystem::path& game_dir, const std::filesystem::path& settings_file) {
  const DisplaySettings defaults = read_game_settings(game_dir).display;
  return read_game_display(settings_file, game_settings_key(game_dir), defaults);
}

bool save_game_display(const std::filesystem::path& game_dir, const std::filesystem::path& settings_file,
                       const DisplaySettings& display, std::string& error) {
  return write_game_display(settings_file, game_settings_key(game_dir), display, error);
}

}  // namespace redgem
