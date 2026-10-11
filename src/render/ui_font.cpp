#include "render/ui_font.hpp"

REDGEM_EMBED_FILE(redgem_font_ui, REDGEM_UI_FONT_FILE);

namespace redgem {

EmbeddedFile embedded_ui_font() {
  // cppcheck-suppress subtractPointers ; the two labels mark the start and the end of one embedded file
  return REDGEM_EMBEDDED_FILE(redgem_font_ui);
}

}  // namespace redgem
