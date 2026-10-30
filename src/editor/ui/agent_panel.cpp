#include "editor/ui/agent_panel.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <utility>

#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/agent_views.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/palette.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

using agent::AgentState;
using agent::EntryKind;

constexpr int kInputLines = 3;           // how tall the prompt box is, in lines of text
constexpr float kPickerWidth = 150.0f;   // pixels
constexpr float kScrollToBottom = 1.0f;  // SetScrollHereY: the last line at the bottom
constexpr float kEntryGap = 6.0f;        // pixels between entries
constexpr const char* kWhitespace = " \t\r\n";
constexpr float kBothSides = 2.0f;       // padding counted above and below
constexpr float kRowsUnderInput = 2.0f;  // the attachments' row and the buttons' row

bool blank(const std::string& text) { return text.find_first_not_of(kWhitespace) == std::string::npos; }

const char* state_text(AgentState state) {
  switch (state) {
    case AgentState::Starting:
    case AgentState::Initializing:
    case AgentState::OpeningSession:
      return tr("editor.agent.state_starting");
    case AgentState::Ready:
      return tr("editor.agent.state_ready");
    case AgentState::Prompting:
      return tr("editor.agent.state_working");
    case AgentState::Failed:
      break;
  }
  return tr("editor.agent.state_failed");
}

ImVec4 state_color(AgentState state) {
  if (state == AgentState::Failed) return palette::kError;
  return state == AgentState::Ready ? palette::kSuccess : palette::kWarning;
}

// The room the prompt box and the buttons under it take.
float composer_height() {
  const ImGuiStyle& style = ImGui::GetStyle();
  const float input = ImGui::GetTextLineHeight() * kInputLines + style.FramePadding.y * kBothSides;
  return input + ImGui::GetFrameHeightWithSpacing() * kRowsUnderInput + style.ItemSpacing.y;
}

// The hint a multiline box cannot show itself, over the empty box.
void draw_hint(const char* hint) {
  const ImVec2 corner = ImGui::GetItemRectMin();
  const ImVec2 padding = ImGui::GetStyle().FramePadding;
  ImGui::GetWindowDrawList()->AddText(ImVec2(corner.x + padding.x, corner.y + padding.y),
                                      ImGui::GetColorU32(palette::kTextDisabled), hint);
}

}  // namespace

AgentPanel::AgentPanel(AgentPanelSetup setup)
    : setup_(std::move(setup)), agents_(agent::read_agents(setup_.agents_file)) {}

AgentPanel::~AgentPanel() = default;

void AgentPanel::pick(std::size_t agent) {
  if (agent >= agents_.size() || agent == picked_) return;
  picked_ = agent;
  end_session();
}

void AgentPanel::start_session() {
  end_session();
  if (agents_.empty()) return;
  const auto call = agent::launch_call(agents_[picked_], setup_.find, start_error_);
  if (!call) return;
  std::string tools_error;
  std::vector<agent::McpServer> servers;
  if (setup_.tool_servers) servers = setup_.tool_servers(tools_error);
  connection_ = std::make_unique<agent::AgentConnection>(
      *call, agent::ClientSetup{setup_.game, std::move(servers), setup_.client_version});
  if (!tools_error.empty()) {
    connection_->client().transcript().add(EntryKind::Notice, tr("editor.agent.no_tools", {{"error", tools_error}}));
  }
}

void AgentPanel::end_session() {
  connection_.reset();  // (stops the agent's program)
  start_error_.clear();
  queued_.clear();
  parsed_.clear();
}

bool AgentPanel::send(const std::string& text) {
  if (blank(text)) return false;
  if (!live()) {
    start_session();
    queued_ = text;
    return connection_ != nullptr;
  }
  if (!connection_->client().prompt(text, attached_)) return false;
  attached_.clear();
  return true;
}

void AgentPanel::stop() {
  if (running()) connection_->client().cancel();
}

void AgentPanel::attach_selection() {
  attached_ = setup_.selection_text ? setup_.selection_text() : std::vector<std::string>{};
  hint_ = attached_.empty() ? tr("editor.agent.nothing_selected") : "";
}

bool AgentPanel::running() const { return connection_ != nullptr && connection_->client().busy(); }

// A conversation that can take a prompt now or once it is ready: started, and not failed.
bool AgentPanel::live() const { return connection_ != nullptr && connection_->client().state() != AgentState::Failed; }

void AgentPanel::poll() {
  if (!connection_) return;
  connection_->poll();
  if (queued_.empty() || connection_->client().state() != AgentState::Ready) return;
  if (connection_->client().prompt(queued_, attached_)) attached_.clear();
  queued_.clear();
}

void AgentPanel::draw() {
  poll();
  const bool shown = ImGui::Begin(window_title(editor_config::kAgentTitle, editor_config::kAgentWindow));
  if (shown) {
    draw_toolbar();
    draw_problem();
    draw_transcript();
    draw_composer();
    draw_settings();
  }
  ImGui::End();
}

void AgentPanel::draw_toolbar() {
  draw_agent_picker();
  ImGui::SameLine();
  if (icon_button(ICON_FA_PLUS, tr("editor.agent.new_session"))) start_session();
  ImGui::SameLine();
  if (icon_button(ICON_FA_GEAR, tr("editor.agent.settings"))) open_settings();
  ImGui::SameLine();
  draw_status();
  ImGui::Separator();
}

void AgentPanel::draw_agent_picker() {
  ImGui::SetNextItemWidth(kPickerWidth);
  const char* shown = agents_.empty() ? "" : agents_[picked_].name.c_str();
  if (!ImGui::BeginCombo("##agent", shown)) return;
  for (std::size_t index = 0; index < agents_.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));
    if (ImGui::Selectable(agents_[index].name.c_str(), index == picked_)) pick(index);
    ImGui::PopID();
  }
  ImGui::EndCombo();
}

void AgentPanel::draw_status() {
  if (!connection_) return ImGui::TextColored(palette::kMuted, "%s", tr("editor.agent.state_none"));
  const AgentState state = connection_->client().state();
  ImGui::TextColored(state_color(state), "%s", ICON_FA_CIRCLE);
  ImGui::SameLine();
  ImGui::TextColored(palette::kMuted, "%s", state_text(state));
}

void AgentPanel::draw_problem() {
  if (!start_error_.empty()) agent_view::draw_error(start_error_);
  if (!connection_ || connection_->client().state() != AgentState::Failed) return;
  const std::string login = agent::login_hint(agents_[picked_]);
  if (connection_->client().needs_login() && !login.empty()) {
    ImGui::TextColored(palette::kWarning, "%s", ICON_FA_TRIANGLE_EXCLAMATION);
    ImGui::SameLine();
    ImGui::TextWrapped("%s", login.c_str());
  }
}

void AgentPanel::draw_transcript() {
  ImGui::BeginChild("transcript", ImVec2(0.0f, -composer_height()));
  const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
  if (connection_) {
    draw_entries(connection_->client());
  } else if (start_error_.empty()) {
    ImGui::TextColored(palette::kMuted, "%s", ICON_FA_ROBOT);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, palette::kMuted);
    ImGui::TextWrapped("%s", tr("editor.agent.no_session"));
    ImGui::PopStyleColor();
  }
  follow_new_entries(at_bottom);
  ImGui::EndChild();
}

// The view stays at the bottom while it is there and more comes.
void AgentPanel::follow_new_entries(bool at_bottom) {
  const std::uint64_t revision = connection_ ? connection_->client().transcript().revision() : 0;
  if (revision != seen_revision_ && at_bottom) ImGui::SetScrollHereY(kScrollToBottom);
  seen_revision_ = revision;
}

void AgentPanel::draw_entries(agent::AgentClient& client) {
  const auto& entries = client.transcript().entries();
  for (std::size_t index = 0; index < entries.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));
    draw_entry(client, index);
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, kEntryGap));
  }
  if (client.busy()) agent_view::draw_working();
}

void AgentPanel::draw_entry(agent::AgentClient& client, std::size_t index) {
  const agent::Entry& entry = client.transcript().entries()[index];
  switch (entry.kind) {
    case EntryKind::Agent:
      return agent_view::draw_markdown(markdown(index, entry.text));
    case EntryKind::Tool:
      return agent_view::draw_tool(entry.tool);
    case EntryKind::Plan:
      return agent_view::draw_plan(entry.plan);
    case EntryKind::Permission:
      return answer(client, index, agent_view::draw_permission(entry.permission));
    default:
      return draw_text_entry(entry);
  }
}

// What the user said, a thought, a notice or an error: text, each in its own look.
void AgentPanel::draw_text_entry(const agent::Entry& entry) {
  if (entry.kind == EntryKind::User) return agent_view::draw_user(entry.text);
  if (entry.kind == EntryKind::Thought) return agent_view::draw_thought(entry.text);
  if (entry.kind == EntryKind::Error) return agent_view::draw_error(entry.text);
  agent_view::draw_notice(entry.text);
}

void AgentPanel::answer(agent::AgentClient& client, std::size_t index, const std::optional<std::string>& option) {
  if (option) client.choose(index, *option);
}

const std::vector<agent::Block>& AgentPanel::markdown(std::size_t entry, const std::string& text) {
  ParsedText& parsed = parsed_[entry];
  if (parsed.length != text.size()) parsed = {text.size(), agent::parse_markdown(text)};
  return parsed.blocks;
}

void AgentPanel::draw_composer() {
  draw_attachments();
  draw_input();
  draw_actions();
}

void AgentPanel::draw_attachments() {
  if (attached_.empty()) {
    ImGui::TextColored(palette::kWarning, "%s", hint_.c_str());
    return;
  }
  const std::string count = std::to_string(attached_.size());
  ImGui::TextColored(palette::kAccentHover, "%s %s", ICON_FA_PAPERCLIP,
                     tr("editor.agent.attached", {{"count", count}}).c_str());
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_FA_XMARK)) attached_.clear();
  tooltip(tr("editor.agent.detach"));
}

void AgentPanel::draw_input() {
  if (std::exchange(focus_input_, false)) ImGui::SetKeyboardFocusHere();
  const float height = ImGui::GetTextLineHeight() * kInputLines + ImGui::GetStyle().FramePadding.y * kBothSides;
  const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine;
  const bool entered = ImGui::InputTextMultiline("##prompt", &input_, ImVec2(-1.0f, height), flags);
  if (input_.empty() && !ImGui::IsItemActive()) draw_hint(tr("editor.agent.input_hint"));
  if (!entered) return;
  if (send(input_)) input_.clear();
  focus_input_ = true;
}

void AgentPanel::draw_actions() {
  if (icon_button(ICON_FA_PAPERCLIP, tr("editor.agent.attach"))) attach_selection();
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * editor_config::kIconButtonWidth);
  if (running()) {
    if (icon_button(ICON_FA_CIRCLE_STOP, tr("editor.agent.stop"))) stop();
    return;
  }
  ImGui::BeginDisabled(blank(input_));
  if (icon_button(ICON_FA_PAPER_PLANE, tr("editor.agent.send")) && send(input_)) input_.clear();
  ImGui::EndDisabled();
}

}  // namespace redgem::ui
