// The editor's fonts (assets/editor/fonts, SIL Open Font License) built into the binary, so a
// standalone build needs no extra files.
#include "editor/ui/embedded_fonts.hpp"

#define REDGEM_EMBED_FILE(symbol, file)                       \
  __asm__(".section .rodata\n"                                   \
          ".global " #symbol "\n"                                \
          ".balign 16\n" #symbol ":\n"                           \
          ".incbin \"" REDGEM_EDITOR_FONT_DIR "/" file "\"\n" \
          ".global " #symbol "_end\n" #symbol "_end:\n"          \
          ".previous\n")

REDGEM_EMBED_FILE(redgem_font_inter, "InterVariable.ttf");
REDGEM_EMBED_FILE(redgem_font_mono, "JetBrainsMono-Regular.ttf");

extern "C" {
extern const unsigned char redgem_font_inter[];
extern const unsigned char redgem_font_inter_end[];
extern const unsigned char redgem_font_mono[];
extern const unsigned char redgem_font_mono_end[];
}

namespace redgem::fonts {

EmbeddedFont interface_font() {
  return {redgem_font_inter, static_cast<int>(redgem_font_inter_end - redgem_font_inter)};
}

EmbeddedFont code_font() {
  return {redgem_font_mono, static_cast<int>(redgem_font_mono_end - redgem_font_mono)};
}

}  // namespace redgem::fonts
