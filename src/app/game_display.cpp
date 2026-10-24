#include "app/game_display.hpp"

#include "app/game_settings.hpp"
#include "app/save_location.hpp"
#include "app/user_settings.hpp"

namespace redgem {

PlayerDisplay player_display(const std::filesystem::path& game_dir, const std::filesystem::path& settings_file) {
  const GameSettings game = read_game_settings(game_dir);
  const std::string key = game_settings_key(game_dir, game);
  return {read_game_display(settings_file, key, game.display), {settings_file, key}};
}

bool keep_display(const DisplayKeeping& kept_in, const DisplaySettings& display, std::string& error) {
  return write_game_display(kept_in.file, kept_in.game_key, display, error);
}

}  // namespace redgem
