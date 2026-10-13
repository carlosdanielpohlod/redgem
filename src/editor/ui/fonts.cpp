// The editor's fonts: Inter for the interface and JetBrains Mono for code, each with the icons.

#include "editor/editor_config.hpp"
#include "editor/ui/embedded_fonts.hpp"
#include "editor/ui/style.hpp"
#include "render/ui_font.hpp"
#include "extras/FA6FreeSolidFontData.h"

namespace redgem::ui {
namespace {

using namespace editor_config;

// The fonts, in the order load_fonts adds them to the interface's own list.
constexpr int kCodeFontSlot = 1;  // after the interface's font, which is the default

// Icons are a bit smaller than the text and nudged down to sit on its baseline.
constexpr float kIconScale = 0.9f;
constexpr float kIconOffsetY = 1.0f;
constexpr float kIconMinAdvance = 1.25f;  // icons get a fixed width (in font sizes) so they line up
constexpr ImWchar kRangesEnd = 0;         // ImGui: closes a list of glyph ranges

// The icons, merged into the font added last.
void merge_icons(float size) {
  static const ImWchar kIconRanges[] = {ICON_MIN_FA, ICON_MAX_FA, kRangesEnd};
  ImFontConfig icons;
  icons.MergeMode = true;
  icons.PixelSnapH = true;
  icons.FontDataOwnedByAtlas = false;
  icons.GlyphMinAdvanceX = size * kIconMinAdvance;
  icons.GlyphOffset = ImVec2(0.0f, kIconOffsetY);
  ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, fa_solid_900_compressed_size,
                                                       size * kIconScale, &icons, kIconRanges);
}

void add_font(const EmbeddedFile& font, float size) {
  ImFontConfig config;
  config.FontDataOwnedByAtlas = false;  // the data lives in the binary
  ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(font.data), font.size, size, &config);
  merge_icons(size);
}

}  // namespace

void load_fonts() {
  add_font(embedded_ui_font(), kInterfaceFontSize);  // the first font is the default
  add_font(fonts::code_font(), kCodeFontSize);       // kCodeFontSlot
}

ImFont* code_font() { return ImGui::GetIO().Fonts->Fonts[kCodeFontSlot]; }

}  // namespace redgem::ui
