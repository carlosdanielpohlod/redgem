#pragma once

#include "core/embed.hpp"

namespace redgem {

// The font texts are drawn with (Inter, SIL Open Font License), built into the program: games
// draw their UI.text with it, and the editor its interface.
EmbeddedFile embedded_ui_font();

}  // namespace redgem
