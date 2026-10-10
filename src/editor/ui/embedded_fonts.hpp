#pragma once

namespace redgem::fonts {

// A TrueType file built into the binary.
struct EmbeddedFont {
  const unsigned char* data = nullptr;
  int size = 0;
};

EmbeddedFont interface_font();  // Inter
EmbeddedFont code_font();       // JetBrains Mono

}  // namespace redgem::fonts
