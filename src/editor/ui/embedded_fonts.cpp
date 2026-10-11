// The editor's code font (assets/editor/fonts, SIL Open Font License) built into the binary, so
// the editor needs no extra files.
#include "editor/ui/embedded_fonts.hpp"

REDGEM_EMBED_FILE(redgem_font_mono, REDGEM_CODE_FONT_FILE);

namespace redgem::fonts {

EmbeddedFile code_font() {
  // cppcheck-suppress subtractPointers ; the two labels mark the start and the end of one embedded file
  return REDGEM_EMBEDDED_FILE(redgem_font_mono);
}

}  // namespace redgem::fonts
