#pragma once

#include <imgui.h>

#include <string>

#include "editor/scripts/ruby_tokens.hpp"

namespace redgem::ui {

// The color a kind of Ruby code is drawn in.
ImVec4 code_color(RubyTokenKind kind);

// Scrolls the code box to the requested one-based source line, clamped to its last line.
void scroll_code_to_line(ImGuiID input_id, std::size_t line, std::size_t line_count);

// Draws `code` in its colors over the multi-line text box with id `input_id`, drawn just before
// with an invisible text color: the box keeps its editing, selection and cursor, and the colored
// copy shows where its own text would. Call while the box's font is still pushed.
void draw_code_colors(ImGuiID input_id, const std::string& code, const RubyHighlight& highlight);

}  // namespace redgem::ui
