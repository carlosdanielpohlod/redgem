#pragma once

#include <imgui.h>

#include "extras/IconsFontAwesome6.h"  // ICON_FA_* (Font Awesome 6 Free Solid, merged into every font)

namespace redgem::ui {

// Fonts of the editor: Inter for the interface, JetBrains Mono for code. Both have the Font
// Awesome icons merged in. Meant to be rlImGui's font-loading callback.
void load_fonts();
// The dark editor theme (colors, spacing, rounding).
void apply_theme();
ImFont* code_font();

// Labels of commands that several menus and dialogs offer.
inline constexpr const char* kSaveLabel = ICON_FA_FLOPPY_DISK "  Save";
inline constexpr const char* kDiscardLabel = "Discard";
inline constexpr const char* kCancelLabel = "Cancel";
inline constexpr const char* kDuplicateLabel = ICON_FA_CLONE "  Duplicate";
inline constexpr const char* kDeleteLabel = ICON_FA_TRASH "  Delete";

// Palette, for panels that draw their own accents.
ImVec4 accent_color();
ImVec4 muted_color();
ImVec4 error_color();
ImVec4 warning_color();
ImVec4 success_color();

// "Label    [widget.......]" rows: the label column, then the next widget fills the rest.
void property_label(const char* text);
// A component card in the Inspector: header with icon and title, open by default. With
// `removed`, the header gets a close button that sets it.
bool component_header(const char* icon, const char* title, bool* removed = nullptr);
// A square icon button with a tooltip; `active` shows it pressed (toggle buttons).
bool icon_button(const char* icon, const char* tooltip, bool active = false);
void tooltip(const char* text);

}  // namespace redgem::ui
