#pragma once

// The editor's texts as its widgets show them (see core/translations.hpp): after an icon, or
// with an ImGui id after them. Each is made the first time it is asked for and kept, so drawing
// it every frame allocates nothing. (The language is chosen before the first window opens.)
namespace redgem::ui {

// "<icon>  <text of key>": a menu item's or a button's label.
const char* icon_label(const char* icon, const char* key);
// "<text of key>##<id>": a text ImGui tells apart from the same text elsewhere in its window.
const char* label_with_id(const char* key, const char* id);
// "<icon>  <text of key>##<id>": both.
const char* icon_label_with_id(const char* icon, const char* key, const char* id);

}  // namespace redgem::ui
