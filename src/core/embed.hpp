#pragma once

// Builds a file into the program, so that what it holds (a font...) needs no file beside the
// executable. The assembler copies the file's bytes in when the program is compiled.
//
//   REDGEM_EMBED_FILE(my_font, "/path/to/font.ttf");      at file scope, in one source file
//   const redgem::EmbeddedFile font = REDGEM_EMBEDDED_FILE(my_font);

// Read-only data is a differently named section in Windows' and in the Mac's object files, and
// their assemblers have no "go back to the previous section": code is what follows. The Mac also
// spells a C name with a leading underscore once it is a label.
#ifdef _WIN32
#define REDGEM_EMBED_READONLY_SECTION ".section .rdata,\"dr\"\n"
#define REDGEM_EMBED_BACK_TO_CODE ".text\n"
#define REDGEM_EMBED_LABEL_PREFIX ""
#elif defined(__APPLE__)
#define REDGEM_EMBED_READONLY_SECTION ".section __TEXT,__const\n"
#define REDGEM_EMBED_BACK_TO_CODE ".text\n"
#define REDGEM_EMBED_LABEL_PREFIX "_"
#else
#define REDGEM_EMBED_READONLY_SECTION ".section .rodata\n"
#define REDGEM_EMBED_BACK_TO_CODE ".previous\n"
#define REDGEM_EMBED_LABEL_PREFIX ""
#endif

#define REDGEM_EMBED_ALIGNMENT "16"  // bytes: where a file's data may start

// The file at `path` (a string literal), between the labels `symbol` and `symbol`_end.
#define REDGEM_EMBED_FILE(symbol, path)                                                                          \
  __asm__(REDGEM_EMBED_READONLY_SECTION ".global " REDGEM_EMBED_LABEL_PREFIX #symbol                             \
                                        "\n"                                                                     \
                                        ".balign " REDGEM_EMBED_ALIGNMENT "\n" REDGEM_EMBED_LABEL_PREFIX #symbol \
                                        ":\n"                                                                    \
                                        ".incbin \"" path                                                        \
                                        "\"\n"                                                                   \
                                        ".global " REDGEM_EMBED_LABEL_PREFIX #symbol                             \
                                        "_end\n" REDGEM_EMBED_LABEL_PREFIX #symbol                               \
                                        "_end:\n" REDGEM_EMBED_BACK_TO_CODE);                                    \
  extern "C" {                                                                                                   \
  extern const unsigned char(symbol)[];                                                                          \
  extern const unsigned char symbol##_end[];                                                                     \
  }                                                                                                              \
  static_assert(true, "")  // (so that the line using the macro ends in a semicolon)

namespace redgem {

// A file built into the program: where its bytes are, and how many.
struct EmbeddedFile {
  const unsigned char* data = nullptr;
  int size = 0;
};

}  // namespace redgem

// The file embedded under `symbol` (the two labels mark the start and the end of one file).
#define REDGEM_EMBEDDED_FILE(symbol) \
  redgem::EmbeddedFile { (symbol), static_cast<int>(symbol##_end - (symbol)) }
