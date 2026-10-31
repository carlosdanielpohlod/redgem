// Ruby code in color. The multi-line text box keeps doing all the editing (typing, undo,
// selection, clipboard); its own text is drawn invisible, and this draws the same text again,
// token by token, where the box would have: in its font, at its scroll, only the lines in view.

#include "editor/ui/code_colors.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <vector>

#include "editor/ui/palette.hpp"

namespace redgem::ui {
namespace {

struct KindColor {
  RubyTokenKind kind;
  ImVec4 color;
};
constexpr KindColor kKindColors[] = {{RubyTokenKind::Keyword, palette::kCodeKeyword},
                                     {RubyTokenKind::String, palette::kCodeString},
                                     {RubyTokenKind::Interpolation, palette::kCodeInterpolation},
                                     {RubyTokenKind::Symbol, palette::kCodeSymbol},
                                     {RubyTokenKind::Regex, palette::kCodeRegex},
                                     {RubyTokenKind::Comment, palette::kCodeComment},
                                     {RubyTokenKind::Number, palette::kCodeNumber},
                                     {RubyTokenKind::Constant, palette::kCodeConstant},
                                     {RubyTokenKind::Variable, palette::kCodeVariable}};

using TokenIt = std::vector<RubyToken>::const_iterator;

// Where and how the box draws its text.
struct CodeView {
  ImDrawList* draw_list;
  ImFont* font;
  float font_size;  // also the height of a line
  ImVec2 origin;    // where the first line's first character goes
  ImVec4 clip;      // the part of the box text shows in (not its scrollbar)
};

// Part of a line drawn in one color: from where the last one ended to `end`.
struct Piece {
  std::size_t end;
  ImU32 color;
};

// The window the multi-line box with this id draws in, a child of the current window.
ImGuiWindow* text_box(ImGuiID input_id) {
  for (ImGuiWindow* child : ImGui::GetCurrentWindowRead()->DC.ChildWindows) {
    if (child->ChildId == input_id) return child;
  }
  return nullptr;
}

// The box is there and in view (not in a collapsed or hidden window).
bool shows_text(const ImGuiWindow* box) { return box != nullptr && !box->SkipItems; }

// The box scrolls its text sideways only while it is typed in or its scrollbar dragged.
bool shows_side_scroll(ImGuiID input_id, const ImGuiInputTextState* state, ImGuiWindow* box) {
  const ImGuiID active = ImGui::GetActiveID();
  return state != nullptr && (active == input_id || active == ImGui::GetWindowScrollbarID(box, ImGuiAxis_Y));
}

float side_scroll(ImGuiID input_id, ImGuiWindow* box) {
  const ImGuiInputTextState* state = ImGui::GetInputTextState(input_id);
  return shows_side_scroll(input_id, state, box) ? state->Scroll.x : 0.0f;
}

CodeView view_of(ImGuiID input_id, ImGuiWindow* box) {
  const ImVec2 padding = ImGui::GetStyle().FramePadding;
  const ImVec2 origin{box->Pos.x + padding.x - box->Scroll.x - side_scroll(input_id, box),
                      box->Pos.y + padding.y - box->Scroll.y};
  return {box->DrawList, ImGui::GetFont(), ImGui::GetFontSize(), origin, box->InnerClipRect.ToVec4()};
}

TokenIt first_token_ending_after(const std::vector<RubyToken>& tokens, std::size_t offset) {
  return std::upper_bound(tokens.begin(), tokens.end(), offset,
                          [](std::size_t at, const RubyToken& token) { return at < token.end; });
}

bool is_plain_at(TokenIt token, TokenIt tokens_end, std::size_t at) { return token == tokens_end || token->begin > at; }

// The next piece of a line from `at`: the token there, or the plain text up to the next token.
Piece next_piece(TokenIt token, TokenIt tokens_end, std::size_t at, std::size_t line_end) {
  if (!is_plain_at(token, tokens_end, at))
    return {std::min(token->end, line_end), ImGui::GetColorU32(code_color(token->kind))};
  const std::size_t end = token == tokens_end ? line_end : std::min(token->begin, line_end);
  return {end, ImGui::GetColorU32(ImGuiCol_Text)};
}

// Draws a piece at `x`; returns where the next one starts.
float draw_piece(const CodeView& view, ImVec2 pos, ImU32 color, const char* begin, const char* end) {
  view.draw_list->AddText(view.font, view.font_size, pos, color, begin, end, 0.0f, &view.clip);
  return pos.x + view.font->CalcTextSizeA(view.font_size, FLT_MAX, 0.0f, begin, end).x;
}

bool token_drawn(TokenIt token, TokenIt tokens_end, std::size_t drawn_to) {
  return token != tokens_end && token->end <= drawn_to;
}

// One line, the bytes [begin, end) of the code, piece by piece until past the box's right edge.
void draw_line(const CodeView& view, const std::string& code, const std::vector<RubyToken>& tokens, std::size_t begin,
               std::size_t end, float y) {
  TokenIt token = first_token_ending_after(tokens, begin);
  ImVec2 pos{view.origin.x, y};
  for (std::size_t at = begin; at < end;) {
    if (pos.x > view.clip.z) return;  // the rest is out of view
    const Piece piece = next_piece(token, tokens.end(), at, end);
    pos.x = draw_piece(view, pos, piece.color, code.data() + at, code.data() + piece.end);
    if (token_drawn(token, tokens.end(), piece.end)) ++token;
    at = piece.end;
  }
}

std::size_t line_count_from(float lines) { return lines > 0.0f ? static_cast<std::size_t>(lines) : 0; }

// The lines of the code in view: from the one at the top edge to the one at the bottom edge.
void draw_lines_in_view(const CodeView& view, const std::string& code, const RubyHighlight& highlight) {
  const std::vector<std::size_t>& starts = highlight.line_starts;
  const std::size_t first = std::min(line_count_from((view.clip.y - view.origin.y) / view.font_size), starts.size());
  const std::size_t last = std::min(line_count_from((view.clip.w - view.origin.y) / view.font_size) + 1, starts.size());
  for (std::size_t line = first; line < last; ++line) {
    const std::size_t end = line + 1 < starts.size() ? starts[line + 1] - 1 : code.size();  // (before its newline)
    draw_line(view, code, highlight.tokens, starts[line], std::min(end, code.size()),
              view.origin.y + static_cast<float>(line) * view.font_size);
  }
}

}  // namespace

ImVec4 code_color(RubyTokenKind kind) {
  for (const KindColor& entry : kKindColors) {
    if (entry.kind == kind) return entry.color;
  }
  return palette::kText;
}

void draw_code_colors(ImGuiID input_id, const std::string& code, const RubyHighlight& highlight) {
  ImGuiWindow* box = text_box(input_id);
  if (!shows_text(box)) return;
  const CodeView view = view_of(input_id, box);
  box->DrawList->PushClipRect(ImVec2(view.clip.x, view.clip.y), ImVec2(view.clip.z, view.clip.w));
  draw_lines_in_view(view, code, highlight);
  box->DrawList->PopClipRect();
}

}  // namespace redgem::ui
