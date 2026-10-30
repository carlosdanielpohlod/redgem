// Part of the Agent panel: the entries of a conversation other than the agent's answers (which are
// markdown): what the user said, thoughts, tool call cards, the plan, questions, notices.

#include <imgui.h>

#include <cmath>
#include <string>

#include "core/translations.hpp"
#include "editor/agent/text_diff.hpp"
#include "editor/ui/agent_views.hpp"
#include "editor/ui/palette.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::agent_view {
namespace {

using agent::DiffMark;
using agent::PermissionKind;
using agent::PermissionRequest;
using agent::PlanStatus;
using agent::ToolCall;
using agent::ToolStatus;

constexpr float kBoxPadding = 6.0f;  // pixels inside the user's message box
constexpr float kBoxRounding = 4.0f;
constexpr int kBoxStyles = 2;           // the style values the box pushes (rounding, padding)
constexpr double kSpinnerPeriod = 0.8;  // seconds per turn of the working mark
constexpr int kSpinnerFrames = 4;
constexpr const char* kSpinner[kSpinnerFrames] = {"|", "/", "-", "\\"};
constexpr float kDetailIndent = 8.0f;  // pixels a tool card's details are set in
constexpr const char* kRemovedMark = "- ";
constexpr const char* kAddedMark = "+ ";
constexpr const char* kKeptMark = "  ";

struct StatusLook {
  const char* icon = nullptr;
  ImVec4 color;
};

StatusLook status_look(ToolStatus status) {
  switch (status) {
    case ToolStatus::Completed:
      return {ICON_FA_CIRCLE_CHECK, palette::kSuccess};
    case ToolStatus::Failed:
      return {ICON_FA_CIRCLE_XMARK, palette::kError};
    case ToolStatus::InProgress:
      return {ICON_FA_CIRCLE_NOTCH, palette::kWarning};
    case ToolStatus::Pending:
      break;
  }
  return {ICON_FA_CLOCK, palette::kMuted};
}

// The icon of each kind of tool call the protocol names.
struct KindIcon {
  const char* kind;
  const char* icon;
};
constexpr KindIcon kKindIcons[] = {{"read", ICON_FA_FILE_LINES},
                                   {"edit", ICON_FA_FILE_PEN},
                                   {"delete", ICON_FA_TRASH},
                                   {"move", ICON_FA_ARROW_RIGHT_ARROW_LEFT},
                                   {"search", ICON_FA_MAGNIFYING_GLASS},
                                   {"execute", ICON_FA_TERMINAL},
                                   {"think", ICON_FA_LIGHTBULB},
                                   {"fetch", ICON_FA_GLOBE}};

const char* kind_icon(const std::string& kind) {
  for (const KindIcon& known : kKindIcons) {
    if (kind == known.kind) return known.icon;
  }
  return ICON_FA_WRENCH;
}

const char* plan_icon(PlanStatus status) {
  if (status == PlanStatus::Completed) return ICON_FA_SQUARE_CHECK;
  return status == PlanStatus::InProgress ? ICON_FA_CIRCLE_HALF_STROKE : ICON_FA_SQUARE;
}

void wrapped(const ImVec4& color, const std::string& text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextWrapped("%s", text.c_str());
  ImGui::PopStyleColor();
}

void code_text(const ImVec4& color, const std::string& text) {
  ImGui::PushFont(code_font(), 0.0f);
  wrapped(color, text);
  ImGui::PopFont();
}

const char* diff_mark(DiffMark mark) {
  if (mark == DiffMark::Removed) return kRemovedMark;
  return mark == DiffMark::Added ? kAddedMark : kKeptMark;
}

ImVec4 diff_color(DiffMark mark) {
  if (mark == DiffMark::Removed) return palette::kError;
  return mark == DiffMark::Added ? palette::kSuccess : palette::kMuted;
}

void draw_change(const agent::FileChange& change) {
  ImGui::TextColored(palette::kMuted, "%s %s", ICON_FA_FILE_PEN, change.path.c_str());
  for (const agent::DiffLine& line : agent::diff_lines(change.old_text.value_or(""), change.new_text)) {
    if (line.mark == DiffMark::Skipped) {
      ImGui::TextColored(palette::kMuted, "%s", tr("editor.agent.more_lines", {{"count", line.text}}).c_str());
      continue;
    }
    code_text(diff_color(line.mark), diff_mark(line.mark) + line.text);
  }
}

void draw_tool_details(const ToolCall& call) {
  ImGui::Indent(kDetailIndent);
  if (!call.input.empty()) code_text(palette::kMuted, call.input);
  for (const agent::ToolContent& content : call.content) {
    if (content.change) {
      draw_change(*content.change);
    } else if (!content.text.empty()) {
      code_text(palette::kText, content.text);
    }
  }
  ImGui::Unindent(kDetailIndent);
}

bool allows(PermissionKind kind) { return kind == PermissionKind::AllowOnce || kind == PermissionKind::AllowAlways; }

const char* option_icon(PermissionKind kind) { return allows(kind) ? ICON_FA_CHECK : ICON_FA_XMARK; }

bool option_button(const agent::PermissionOption& option) {
  const std::string label = std::string(option_icon(option.kind)) + " " + option.name + "##" + option.id;
  if (allows(option.kind)) ImGui::PushStyleColor(ImGuiCol_Button, palette::kAccentSoft);
  const bool clicked = ImGui::Button(label.c_str());
  if (allows(option.kind)) ImGui::PopStyleColor();
  return clicked;
}

void draw_answer(const PermissionRequest& request) {
  const agent::PermissionOption* chosen = request.chosen();
  if (chosen == nullptr) return ImGui::TextColored(palette::kMuted, "%s", tr("editor.agent.question_cancelled"));
  const ImVec4 color = allows(chosen->kind) ? palette::kSuccess : palette::kError;
  ImGui::TextColored(color, "%s", tr("editor.agent.question_answered", {{"option", chosen->name}}).c_str());
}

// A button per option, on one row; the id of the one clicked.
std::optional<std::string> draw_options(const PermissionRequest& request) {
  std::optional<std::string> chosen;
  for (const agent::PermissionOption& option : request.options()) {
    if (option_button(option)) chosen = option.id;
    ImGui::SameLine();
  }
  ImGui::NewLine();
  return chosen;
}

}  // namespace

void draw_user(const std::string& text) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, palette::kFrame);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kBoxRounding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kBoxPadding, kBoxPadding));
  ImGui::BeginChild("user", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
  ImGui::TextColored(palette::kAccentHover, "%s", ICON_FA_USER);
  ImGui::SameLine();
  ImGui::TextWrapped("%s", text.c_str());
  ImGui::EndChild();
  ImGui::PopStyleVar(kBoxStyles);
  ImGui::PopStyleColor();
}

void draw_thought(const std::string& text) {
  const std::string label = std::string(ICON_FA_BRAIN) + " " + tr("editor.agent.thinking") + "##thought";
  ImGui::PushStyleColor(ImGuiCol_Text, palette::kMuted);
  const bool open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
  ImGui::PopStyleColor();
  if (!open) return;
  wrapped(palette::kMuted, text);
  ImGui::TreePop();
}

void draw_tool(const ToolCall& call) {
  const StatusLook look = status_look(call.status);
  ImGui::TextColored(look.color, "%s", look.icon);
  ImGui::SameLine();
  const std::string title = call.title.empty() ? tr("editor.agent.tool") : call.title;
  const std::string label = std::string(kind_icon(call.kind)) + " " + title + "###tool";
  const bool open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
  if (!open) return;
  draw_tool_details(call);
  ImGui::TreePop();
}

void draw_plan(const std::vector<agent::PlanItem>& plan) {
  ImGui::TextColored(palette::kMuted, "%s %s", ICON_FA_LIST_CHECK, tr("editor.agent.plan"));
  for (const agent::PlanItem& item : plan) {
    const bool done = item.status == PlanStatus::Completed;
    ImGui::TextColored(done ? palette::kSuccess : palette::kText, "%s", plan_icon(item.status));
    ImGui::SameLine();
    wrapped(done ? palette::kMuted : palette::kText, item.content);
  }
}

std::optional<std::string> draw_permission(const PermissionRequest& request) {
  const std::string title = request.tool().title.value_or(tr("editor.agent.tool"));
  ImGui::TextColored(palette::kWarning, "%s", ICON_FA_SHIELD_HALVED);
  ImGui::SameLine();
  wrapped(palette::kText, tr("editor.agent.question", {{"tool", title}}));
  if (!request.waiting()) {
    draw_answer(request);
    return std::nullopt;
  }
  return draw_options(request);
}

void draw_notice(const std::string& text) { wrapped(palette::kMuted, text); }

void draw_error(const std::string& text) {
  ImGui::TextColored(palette::kError, "%s", ICON_FA_CIRCLE_XMARK);
  ImGui::SameLine();
  wrapped(palette::kError, text);
}

void draw_working() {
  const int frame = static_cast<int>(std::fmod(ImGui::GetTime() / kSpinnerPeriod, 1.0) * kSpinnerFrames);
  ImGui::TextColored(palette::kAccentHover, "%s %s", kSpinner[frame % kSpinnerFrames], tr("editor.agent.working"));
}

}  // namespace redgem::ui::agent_view
