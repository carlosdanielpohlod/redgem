#include "app/game_texts.hpp"

#include "core/translations.hpp"
#include "generated/game_texts.hpp"

namespace redgem {

void add_game_texts() { translations::add_files(generated::game_texts()); }

}  // namespace redgem
