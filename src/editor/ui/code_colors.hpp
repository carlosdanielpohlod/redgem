#pragma once

#include <imgui.h>

#include <string>

#include "editor/scripts/ruby_tokens.hpp"

namespace redgem::ui {

// The color a kind of Ruby code is drawn in.
ImVec4 code_color(RubyTokenKind kind);

// Draws `code` in its colors over the multi-line text box with id `input_id`, drawn just before
// with an invisible text color: the box keeps its editing, selection and cursor, and the colored
// copy shows where its own text would. Call while the box's font is still pushed.
void draw_code_colors(ImGuiID input_id, const std::string& code, const RubyHighlight& highlight);

}  // namespace redgem::ui
