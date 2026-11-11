#include "app/game_display.hpp"

#include "app/game_settings.hpp"
#include "app/user_settings.hpp"

namespace redgem {

PlayerDisplay player_display(const std::filesystem::path& game_dir, const std::filesystem::path& settings_file) {
  return {read_game_display(settings_file, read_game_settings(game_dir).display), {settings_file}};
}

bool keep_display(const DisplayKeeping& kept_in, const DisplaySettings& display, std::string& error) {
  return write_game_display(kept_in.file, display, error);
}

}  // namespace redgem
