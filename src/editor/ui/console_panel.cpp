#include <imgui.h>

#include <algorithm>

#include "editor/editor_config.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

constexpr float kScrollToBottom = 1.0f;  // SetScrollHereY: align the last line with the bottom
constexpr const char* kLogId = "log";
constexpr const char* kIdSeparator = "##";  // ImGui: what follows names the widget without being shown

struct LevelStyle {
  const char* icon;
  ImVec4 color;
};

LevelStyle style_of(log::Level level) {
  switch (level) {
    case log::Level::Error: return {ICON_FA_CIRCLE_XMARK, error_color()};
    case log::Level::Warning: return {ICON_FA_TRIANGLE_EXCLAMATION, warning_color()};
    case log::Level::Debug: return {ICON_FA_BUG, muted_color()};
    case log::Level::Info: break;
  }
  return {ICON_FA_CIRCLE_INFO, muted_color()};
}

// Which kinds of messages are listed (the buttons at the top toggle them).
struct Filters {
  bool info = true;
  bool warnings = true;
  bool errors = true;

  bool shows(log::Level level) const {
    if (level == log::Level::Error) return errors;
    return level == log::Level::Warning ? warnings : info;
  }
};
Filters g_filters;

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

void draw_filters(ConsoleLog& console) {
  if (ImGui::SmallButton(ICON_FA_BAN "  Clear")) console.clear();
  const long long info = count_of(console, log::Level::Info) + count_of(console, log::Level::Debug);
  filter_button(ICON_FA_CIRCLE_INFO, info, g_filters.info, ImGui::GetStyleColorVec4(ImGuiCol_Text));
  filter_button(ICON_FA_TRIANGLE_EXCLAMATION, count_of(console, log::Level::Warning), g_filters.warnings, warning_color());
  filter_button(ICON_FA_CIRCLE_XMARK, count_of(console, log::Level::Error), g_filters.errors, error_color());
}

void draw_entry(const ConsoleEntry& entry) {
  const LevelStyle style = style_of(entry.level);
  ImGui::TextColored(style.color, "%s", style.icon);
  ImGui::SameLine();
  if (entry.level == log::Level::Info) {
    ImGui::TextUnformatted(entry.message.c_str());
  } else {
    ImGui::TextColored(style.color, "%s", entry.message.c_str());
  }
}

}  // namespace

void draw_console(ConsoleLog& console) {
  ImGui::Begin(editor_config::kConsoleWindow);
  draw_filters(console);
  ImGui::Separator();

  ImGui::BeginChild(kLogId, ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
  const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
  ImGui::PushFont(code_font(), 0.0f);
  for (const ConsoleEntry& entry : console.entries()) {
    if (g_filters.shows(entry.level)) draw_entry(entry);
  }
  ImGui::PopFont();
  if (at_bottom) ImGui::SetScrollHereY(kScrollToBottom);
  ImGui::EndChild();
  ImGui::End();
}

}  // namespace redgem::ui
