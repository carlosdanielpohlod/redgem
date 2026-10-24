#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include "app/game_identity.hpp"
#include "app/presenter.hpp"

// The splash picture a game's game.json names ("splash": "images/splash.png"), shown over its
// whole window while the game gets ready: from when the window opens until the game's first frame
// is drawn over it (its world, its scripts and the upload of its first scene to the graphics card
// happen in between).
namespace redgem {

// What the picture's file holds, when the game names one that is a PNG inside its folder that can
// be decoded; nullopt otherwise (with a warning for one that is named and cannot be used).
std::optional<std::vector<std::uint8_t>> splash_picture(const std::filesystem::path& game_dir,
                                                        const GameIdentity& identity);

// Shows the game's splash picture, fitted to the window with black bars around it, when the game
// is played in a window of its own (a built game, --play, a phone). Nothing for anyone else: the
// editor plays games inside itself, tests and servers have no window.
void show_splash(Presenter* presenter, const std::filesystem::path& game_dir, const GameIdentity& identity);

}  // namespace redgem
