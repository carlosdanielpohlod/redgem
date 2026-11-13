#include <imgui.h>

#include <algorithm>

#include "editor/editor_config.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

constexpr float kScrollToBottom = 1.0f;  // SetScrollHereY: align the last line with the bottom
constexpr const char* kLogId = "log";
constexpr const char* kIdSeparator = "##";  // ImGui: what follows names the widget without being shown

struct LevelStyle {
  const char* icon = nullptr;
  ImVec4 color;
};

LevelStyle style_of(log::Level level) {
  switch (level) {
    case log::Level::Error:
      return {ICON_FA_CIRCLE_XMARK, error_color()};
    case log::Level::Warning:
      return {ICON_FA_TRIANGLE_EXCLAMATION, warning_color()};
    case log::Level::Debug:
      return {ICON_FA_BUG, muted_color()};
    case log::Level::Info:
      break;
  }
  return {ICON_FA_CIRCLE_INFO, muted_color()};
}

long long count_of(const ConsoleLog& console, log::Level level) {
  return std::count_if(console.entries().begin(), console.entries().end(),
                       [level](const ConsoleEntry& entry) { return entry.level == level; });
}

// A button showing how many messages of a kind there are; clicking it hides or shows them.
void filter_button(const char* icon, long long count, bool& shown, ImVec4 color) {
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, shown ? color : muted_color());
  const std::string label = std::string(icon) + " " + std::to_string(count) + kIdSeparator + icon;
  if (ImGui::SmallButton(label.c_str())) shown = !shown;
  ImGui::PopStyleColor();
}

void draw_filters(ConsoleLog& console, ConsoleFilters& filters) {
  if (ImGui::SmallButton(icon_label(ICON_FA_BAN, "editor.console.clear"))) console.clear();
  const long long info = count_of(console, log::Level::Info) + count_of(console, log::Level::Debug);
  filter_button(ICON_FA_CIRCLE_INFO, info, filters.info, ImGui::GetStyleColorVec4(ImGuiCol_Text));
  filter_button(ICON_FA_TRIANGLE_EXCLAMATION, count_of(console, log::Level::Warning), filters.warnings,
                warning_color());
  filter_button(ICON_FA_CIRCLE_XMARK, count_of(console, log::Level::Error), filters.errors, error_color());
}

struct VisibleRow {
  const ConsoleEntry* entry;
  const ConsoleRow* row;
};

std::vector<VisibleRow> visible_rows(const ConsoleLog& console, const ConsoleFilters& filters) {
  std::vector<VisibleRow> rows;
  for (const auto& entry : console.entries()) {
    if (!filters.shows(entry.level)) continue;
    for (const auto& row : entry.rows) rows.push_back({&entry, &row});
  }
  return rows;
}

struct ConsoleInteraction {
  const ConsoleScriptLine* selected = nullptr;
  bool link_hovered = false;
};

bool source_exists(const ConsoleScriptLine& source, const ScriptWorkspace& scripts) {
  try {
    return !project_script_source(scripts.scripts_dir(), std::filesystem::u8path(source.file)).empty();
  } catch (const std::filesystem::filesystem_error&) {
    return false;
  }
}

const ConsoleScriptLine* project_link(const ConsoleRow& row, const ScriptWorkspace& scripts) {
  if (!row.source) return nullptr;
  if (!row.source_exists) row.source_exists = source_exists(*row.source, scripts);
  return row.source_exists.value_or(false) ? &*row.source : nullptr;
}

void draw_source_link(const ConsoleScriptLine& source, std::string_view text, ConsoleInteraction& interaction) {
  const auto origin = ImGui::GetCursorScreenPos();
  const auto size = ImGui::CalcTextSize(text.data(), text.data() + text.size());
  if (ImGui::InvisibleButton("source", size, ImGuiButtonFlags_EnableNav)) interaction.selected = &source;
  if (ImGui::IsItemHovered()) {
    interaction.link_hovered = true;
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  ImGui::GetWindowDrawList()->AddText(origin, ImGui::GetColorU32(ImGuiCol_TextLink), text.data(),
                                      text.data() + text.size());
}

void draw_row_text(const VisibleRow& row, const ScriptWorkspace& scripts, ConsoleInteraction& interaction) {
  const auto text = std::string_view(row.entry->message).substr(row.row->offset, row.row->length);
  if (const auto* source = project_link(*row.row, scripts)) {
    draw_source_link(*source, text, interaction);
  } else if (text.empty()) {
    ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
  } else {
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
  }
}

void draw_row(const VisibleRow& row, const ScriptWorkspace& scripts, ConsoleInteraction& interaction) {
  const auto style = style_of(row.entry->level);
  if (row.row->offset == 0) {
    ImGui::TextColored(style.color, "%s", style.icon);
    ImGui::SameLine();
  }
  if (row.entry->level != log::Level::Info) ImGui::PushStyleColor(ImGuiCol_Text, style.color);
  draw_row_text(row, scripts, interaction);
  if (row.entry->level != log::Level::Info) ImGui::PopStyleColor();
}

void draw_visible_row(const VisibleRow& row, const ScriptWorkspace& scripts, ConsoleInteraction& interaction) {
  ImGui::PushID(row.entry);
  ImGui::PushID(row.row);
  draw_row(row, scripts, interaction);
  ImGui::PopID();
  ImGui::PopID();
}

ConsoleInteraction draw_entries(const ConsoleLog& console, const ConsoleFilters& filters,
                                const ScriptWorkspace& scripts) {
  ConsoleInteraction interaction;
  const auto rows = visible_rows(console, filters);
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(rows.size()), ImGui::GetTextLineHeightWithSpacing());
  while (clipper.Step()) {
    for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
      draw_visible_row(rows[static_cast<std::size_t>(index)], scripts, interaction);
    }
  }
  return interaction;
}

void open_source(ScriptWorkspace& scripts, const ConsoleScriptLine& selected) {
  try {
    std::string error;
    if (!scripts.open_at(std::filesystem::u8path(selected.file), selected.line, error)) log::warning(error);
  } catch (const std::filesystem::filesystem_error&) {
    log::warning("cannot read the script path in the backtrace");
  }
}

bool follows_output(bool at_bottom, const ConsoleInteraction& interaction) {
  return at_bottom && !interaction.link_hovered;
}

ConsoleInteraction draw_log(const ConsoleLog& console, const ConsoleFilters& filters, const ScriptWorkspace& scripts) {
  ImGui::BeginChild(kLogId, ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
  const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
  ImGui::PushFont(code_font(), 0.0f);
  const auto interaction = draw_entries(console, filters, scripts);
  ImGui::PopFont();
  if (follows_output(at_bottom, interaction)) ImGui::SetScrollHereY(kScrollToBottom);
  ImGui::EndChild();
  return interaction;
}

}  // namespace

void draw_console(ConsoleLog& console, ConsoleState& state, ScriptWorkspace& scripts) {
  ImGui::Begin(window_title(editor_config::kConsoleTitle, editor_config::kConsoleWindow));
  draw_filters(console, state.filters);
  ImGui::Separator();
  const auto interaction = draw_log(console, state.filters, scripts);
  ImGui::End();
  if (interaction.selected != nullptr) open_source(scripts, *interaction.selected);
}

}  // namespace redgem::ui
