#pragma once

#include "core/embed.hpp"

namespace redgem::fonts {

// The font the editor shows code in (JetBrains Mono), built into the program. (The interface's
// own font is the one games draw texts with: render/ui_font.hpp.)
EmbeddedFile code_font();

}  // namespace redgem::fonts
