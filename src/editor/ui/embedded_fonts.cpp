// The editor's fonts (assets/editor/fonts, SIL Open Font License) built into the binary, so a
// standalone build needs no extra files.
#include "editor/ui/embedded_fonts.hpp"

// Read-only data is a differently named section in Windows' object files, and their assembler has
// no "go back to the previous section": code is what follows.
#ifdef _WIN32
#define REDGEM_READONLY_SECTION ".section .rdata,\"dr\"\n"
#define REDGEM_BACK_TO_CODE ".text\n"
#else
#define REDGEM_READONLY_SECTION ".section .rodata\n"
#define REDGEM_BACK_TO_CODE ".previous\n"
#endif

#define REDGEM_EMBED_FILE(symbol, file)                       \
  __asm__(REDGEM_READONLY_SECTION                                \
          ".global " #symbol "\n"                                \
          ".balign 16\n" #symbol ":\n"                           \
          ".incbin \"" REDGEM_EDITOR_FONT_DIR "/" file "\"\n" \
          ".global " #symbol "_end\n" #symbol "_end:\n"          \
          REDGEM_BACK_TO_CODE)

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
