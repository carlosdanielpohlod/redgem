#pragma once

namespace redgem {

// The texts players read that the engine shows them (locales/game/): the labels of a game's
// menus, and the texts a game's scripts take from the engine when it has none of its own.
// A program that shows them adds them as it starts (see core/translations.hpp).
void add_game_texts();

}  // namespace redgem
