#include <imgui.h>

#include <algorithm>

#include "editor/editor_config.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

constexpr float kScrollToBottom = 1.0f;  // SetScrollHereY: align the last line with the bottom

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

}  // namespace

void draw_console(ConsoleLog& console) {
  static bool show_info = true;
  static bool show_warnings = true;
  static bool show_errors = true;
  ImGui::Begin(editor_config::kConsoleWindow);
  if (ImGui::SmallButton(ICON_FA_BAN "  Clear")) console.clear();
  const auto count = [&console](log::Level level) {
    return std::count_if(console.entries().begin(), console.entries().end(),
                         [level](const ConsoleEntry& entry) { return entry.level == level; });
  };
  const auto toggle = [](const char* icon, long long n, bool& shown, ImVec4 color) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, shown ? color : muted_color());
    const std::string label = std::string(icon) + " " + std::to_string(n) + "##" + icon;
    if (ImGui::SmallButton(label.c_str())) shown = !shown;
    ImGui::PopStyleColor();
  };
  toggle(ICON_FA_CIRCLE_INFO, count(log::Level::Info) + count(log::Level::Debug), show_info, ImGui::GetStyleColorVec4(ImGuiCol_Text));
  toggle(ICON_FA_TRIANGLE_EXCLAMATION, count(log::Level::Warning), show_warnings, warning_color());
  toggle(ICON_FA_CIRCLE_XMARK, count(log::Level::Error), show_errors, error_color());
  ImGui::Separator();

  ImGui::BeginChild("log", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
  const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
  ImGui::PushFont(code_font(), 0.0f);
  for (const ConsoleEntry& entry : console.entries()) {
    const bool shown = entry.level == log::Level::Error     ? show_errors
                       : entry.level == log::Level::Warning ? show_warnings
                                                            : show_info;
    if (!shown) continue;
    const LevelStyle style = style_of(entry.level);
    ImGui::TextColored(style.color, "%s", style.icon);
    ImGui::SameLine();
    if (entry.level == log::Level::Info) {
      ImGui::TextUnformatted(entry.message.c_str());
    } else {
      ImGui::TextColored(style.color, "%s", entry.message.c_str());
    }
  }
  ImGui::PopFont();
  if (at_bottom) ImGui::SetScrollHereY(kScrollToBottom);
  ImGui::EndChild();
  ImGui::End();
}

}  // namespace redgem::ui
