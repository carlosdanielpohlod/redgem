// Part of the Agent panel: an agent's markdown, drawn block by block. Text with inline styles is
// laid out word by word, so that bold, italic and code sit inside wrapped lines.

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <string>

#include "core/translations.hpp"
#include "editor/ui/agent_views.hpp"
#include "editor/ui/palette.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::agent_view {
namespace {

using agent::Block;
using agent::BlockKind;
using agent::Span;

constexpr float kListIndent = 18.0f;    // pixels per level of a list
constexpr float kMarkGap = 5.0f;        // pixels between a list item's mark and its text
constexpr float kQuoteIndent = 12.0f;   // pixels a quote is set in, past its bar
constexpr float kQuoteBarWidth = 3.0f;  // pixels
constexpr float kCodePadding = 3.0f;    // pixels around inline code
constexpr float kBoldOffset = 1.0f;     // bold is drawn twice, this far apart (the font has no bold face)
constexpr float kHeadingGap = 4.0f;     // pixels above a heading
constexpr float kCodeBlockPadding = 6.0f;
constexpr const char* kBullet = "\xe2\x80\xa2";  // the bullet of a list item
constexpr char kSpace = ' ';

// Where the next word goes, while text is laid out in a band of the window.
struct Flow {
  float left = 0.0f;
  float right = 0.0f;
  float x = 0.0f;
  float y = 0.0f;
  float top = 0.0f;
  float line_height = 0.0f;
};

Flow start_flow(float indent) {
  const ImVec2 start = ImGui::GetCursorScreenPos();
  Flow flow;
  flow.left = start.x + indent;
  flow.right = start.x + ImGui::GetContentRegionAvail().x;
  flow.x = flow.left;
  flow.y = start.y;
  flow.top = start.y;
  flow.line_height = ImGui::GetTextLineHeightWithSpacing();
  return flow;
}

// Reserves the room the flow took, so what comes next goes under it.
void end_flow(const Flow& flow) { ImGui::Dummy(ImVec2(flow.right - flow.left, flow.y + flow.line_height - flow.top)); }

ImVec4 span_color(const Span& span) {
  if (span.code) return palette::kAccentHover;
  return span.italic ? palette::kMuted : palette::kText;
}

// Whether a word fits on the line it would go on (a word wider than the band starts a line alone).
bool wraps(const Flow& flow, float width) { return flow.x > flow.left && flow.x + width > flow.right; }

// Where a word of `width` goes: on the line, or at the start of the next one.
ImVec2 place(Flow& flow, float width) {
  if (wraps(flow, width)) {
    flow.x = flow.left;
    flow.y += flow.line_height;
  }
  const ImVec2 at(flow.x, flow.y);
  flow.x += width;
  return at;
}

void paint_word(const ImVec2& at, float width, const std::string& word, const Span& span) {
  ImFont* font = span.code ? code_font() : ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  if (span.code) {
    draw->AddRectFilled(ImVec2(at.x - kCodePadding, at.y), ImVec2(at.x + width + kCodePadding, at.y + size),
                        ImGui::GetColorU32(palette::kFrame));
  }
  const ImU32 color = ImGui::GetColorU32(span_color(span));
  draw->AddText(font, size, at, color, word.c_str());
  if (span.bold) draw->AddText(font, size, ImVec2(at.x + kBoldOffset, at.y), color, word.c_str());
}

float word_width(ImFont* font, const std::string& word) {
  return font->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, word.c_str()).x;
}

// How much of a word too wide for a whole line fits on one (a character at least).
std::size_t fitting(ImFont* font, const std::string& word, float band) {
  const char* end = font->CalcWordWrapPosition(ImGui::GetFontSize(), word.c_str(), word.c_str() + word.size(), band);
  return std::max<std::size_t>(1, static_cast<std::size_t>(end - word.c_str()));
}

bool too_wide(ImFont* font, const std::string& word, float band) {
  return word.size() > 1 && word_width(font, word) > band;
}

// A word, broken over lines when it is wider than the band (a long path or address).
void draw_word(Flow& flow, std::string word, const Span& span) {
  ImFont* font = span.code ? code_font() : ImGui::GetFont();
  const float band = flow.right - flow.left;
  while (too_wide(font, word, band)) {
    const std::string head = word.substr(0, fitting(font, word, band));
    paint_word(place(flow, band + 1.0f), word_width(font, head), head, span);  // (a line of its own)
    word.erase(0, head.size());
  }
  const float width = word_width(font, word);
  paint_word(place(flow, width), width, word, span);
}

// Each word of a span with the spaces after it, so that a line breaks between words.
void draw_span(Flow& flow, const Span& span) {
  std::size_t start = 0;
  while (start < span.text.size()) {
    const std::size_t space = span.text.find(kSpace, start);
    const std::size_t end = space == std::string::npos ? span.text.size() : span.text.find_first_not_of(kSpace, space);
    const std::size_t stop = end == std::string::npos ? span.text.size() : end;
    draw_word(flow, span.text.substr(start, stop - start), span);
    start = stop;
  }
}

void draw_spans(const std::vector<Span>& spans, float indent) {
  Flow flow = start_flow(indent);
  for (const Span& span : spans) draw_span(flow, span);
  end_flow(flow);
}

// The mark ("•", "12.") before the item's text, set in by the item's level and wide enough for it.
void draw_item(const Block& block, const std::string& mark) {
  const float mark_width = ImGui::CalcTextSize(mark.c_str()).x + kMarkGap;
  const float indent = kListIndent * static_cast<float>(block.level) + std::max(kListIndent, mark_width);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddText(ImVec2(start.x + indent - mark_width, start.y),
                                      ImGui::GetColorU32(palette::kMuted), mark.c_str());
  draw_spans(block.spans, indent);
}

void draw_quote(const Block& block) {
  const ImVec2 start = ImGui::GetCursorScreenPos();
  draw_spans(block.spans, kQuoteIndent);
  const float bottom = ImGui::GetCursorScreenPos().y;
  ImGui::GetWindowDrawList()->AddRectFilled(start, ImVec2(start.x + kQuoteBarWidth, bottom),
                                            ImGui::GetColorU32(palette::kBorder));
}

void draw_heading(const Block& block) {
  ImGui::Dummy(ImVec2(0.0f, kHeadingGap));
  std::vector<Span> spans = block.spans;
  for (Span& span : spans) span.bold = true;
  draw_spans(spans, 0.0f);
}

// Its language, and the button that copies it.
void draw_code_header(const Block& block) {
  ImGui::TextColored(palette::kMuted, "%s", block.language.c_str());
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight());
  if (ImGui::SmallButton(ICON_FA_COPY)) ImGui::SetClipboardText(block.code.c_str());
  tooltip(tr("editor.agent.copy_code"));
}

// A fenced block: its text in the code font on a darker ground, with a button that copies it.
void draw_code(const Block& block, int index) {
  ImGui::PushID(index);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, palette::kPanelDark);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kCodeBlockPadding, kCodeBlockPadding));
  ImGui::BeginChild("code", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_HorizontalScrollbar);
  draw_code_header(block);
  ImGui::PushFont(code_font(), 0.0f);
  ImGui::TextUnformatted(block.code.c_str());
  ImGui::PopFont();
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  ImGui::PopID();
}

// List items and quotes: text set in from the margin, behind a mark.
void draw_set_in(const Block& block) {
  if (block.kind == BlockKind::Quote) return draw_quote(block);
  draw_item(block, block.kind == BlockKind::Bullet ? kBullet : block.number + ".");
}

void draw_block(const Block& block, int index) {
  switch (block.kind) {
    case BlockKind::Code:
      return draw_code(block, index);
    case BlockKind::Heading:
      return draw_heading(block);
    case BlockKind::Rule:
      return ImGui::Separator();
    case BlockKind::Paragraph:
      return draw_spans(block.spans, 0.0f);
    default:
      return draw_set_in(block);
  }
}

}  // namespace

void draw_markdown(const std::vector<Block>& blocks) {
  for (std::size_t index = 0; index < blocks.size(); ++index) draw_block(blocks[index], static_cast<int>(index));
}

}  // namespace redgem::ui::agent_view
