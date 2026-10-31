#include "editor/ui/agent_panel.hpp"

#include <imgui.h>
#include <imgui_internal.h>
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
// While an answer streams in, its markdown is read again at most this often (seconds): reading a
// long answer again for every piece would cost the frame more and more.
constexpr double kReparseEvery = 0.1;
constexpr float kNotMeasured = 0.0f;
constexpr const char* kPromptId = "##prompt";
constexpr char kLineEnd = '\n';  // the height of an entry never drawn yet

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

float input_height() {
  return ImGui::GetTextLineHeight() * kInputLines + ImGui::GetStyle().FramePadding.y * kBothSides;
}

// The room the prompt box and the buttons under it take.
float composer_height() {
  return input_height() + ImGui::GetFrameHeightWithSpacing() * kRowsUnderInput + ImGui::GetStyle().ItemSpacing.y;
}

// Whether markdown read when the text was `parsed_length` long is to be read again now.
bool stale(const ParsedText& parsed, const std::string& text, double now) {
  return parsed.length != text.size() && now - parsed.when >= kReparseEvery;
}

// The hint a multiline box cannot show itself, over the empty box.
void draw_hint(const char* hint) {
  const ImVec2 corner = ImGui::GetItemRectMin();
  const ImVec2 padding = ImGui::GetStyle().FramePadding;
  ImGui::GetWindowDrawList()->AddText(ImVec2(corner.x + padding.x, corner.y + padding.y),
                                      ImGui::GetColorU32(palette::kTextDisabled), hint);
}

// An entry drawn before, `height` tall, that would not show where it stands.
bool out_of_sight(float height) {
  return height > kNotMeasured && !ImGui::IsRectVisible(ImVec2(ImGui::GetContentRegionAvail().x, height));
}

// The transcript shows its end (give or take a line): it stays there while more comes, and stays
// where the user scrolled to otherwise.
bool scrolled_to_bottom() { return ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - ImGui::GetTextLineHeight(); }

}  // namespace

AgentPanel::AgentPanel(AgentPanelSetup setup)
    : setup_(std::move(setup)), agents_(agent::read_agents(setup_.agents_file)) {}

AgentPanel::~AgentPanel() = default;

bool AgentPanel::pickable(std::size_t agent) const { return agent < agents_.size() && agent != picked_; }

void AgentPanel::pick(std::size_t agent) {
  if (!pickable(agent)) return;
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
  heights_.clear();
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
  if (queued_.empty()) return;
  if (connection_->client().state() == AgentState::Failed) return give_back_queued();
  if (connection_->client().state() != AgentState::Ready) return;
  if (connection_->client().prompt(queued_, attached_)) attached_.clear();
  queued_.clear();
}

// A prompt the session failed before taking goes back into the box, to send again (once the box is
// drawn: see take_back_returned).
void AgentPanel::give_back_queued() {
  returned_ = std::move(queued_);
  queued_.clear();
}

// The box is let go of first when it is being typed in: ImGui would put back what it holds over
// what is written into input_ meanwhile.
void AgentPanel::take_back_returned() {
  if (returned_.empty()) return;
  if (ImGui::GetActiveID() == ImGui::GetID(kPromptId)) ImGui::ClearActiveID();
  input_ = input_.empty() ? returned_ : returned_ + kLineEnd + input_;
  returned_.clear();
}

void AgentPanel::draw() {
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
  if (!wants_login()) return;
  ImGui::TextColored(palette::kWarning, "%s", ICON_FA_TRIANGLE_EXCLAMATION);
  ImGui::SameLine();
  ImGui::TextWrapped("%s", agent::login_hint(agents_[picked_]).c_str());
}

// The session failed because the agent wants its user to log in, and the panel knows how.
bool AgentPanel::wants_login() const {
  if (connection_ == nullptr || !connection_->client().needs_login()) return false;
  return !agent::login_hint(agents_[picked_]).empty();
}

void AgentPanel::draw_transcript() {
  ImGui::BeginChild("transcript", ImVec2(0.0f, -composer_height()));
  const bool at_bottom = scrolled_to_bottom();
  if (connection_) {
    draw_entries(connection_->client());
  } else if (start_error_.empty()) {
    ImGui::TextColored(palette::kMuted, "%s", ICON_FA_ROBOT);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, palette::kMuted);
    ImGui::TextWrapped("%s", tr("editor.agent.no_session"));
    ImGui::PopStyleColor();
  }
  // (Every frame, not only when an entry came: an answer keeps growing after it came, as its markdown
  // is read again and the heights of entries out of sight are measured.)
  if (at_bottom) ImGui::SetScrollHereY(kScrollToBottom);
  ImGui::EndChild();
}

void AgentPanel::draw_entries(agent::AgentClient& client) {
  // (Widgets of one session's entries are told apart from another's: what was opened in the last
  // session stays closed in this one.)
  ImGui::PushID(connection_.get());
  const auto& entries = client.transcript().entries();
  if (!entries.empty()) forget_before(entries.front().number);
  forget_heights_if_resized();
  for (std::size_t index = 0; index < entries.size(); ++index) {
    ImGui::PushID(static_cast<int>(entries[index].number));
    draw_or_pass(client, index);
    ImGui::PopID();
  }
  if (client.busy()) agent_view::draw_working();
  ImGui::PopID();
}

// An entry out of sight takes its room alone (as tall as it was last drawn), so that a long
// conversation costs a frame what shows of it.
void AgentPanel::draw_or_pass(agent::AgentClient& client, std::size_t index) {
  const std::size_t number = client.transcript().entries()[index].number;
  const auto known = heights_.find(number);
  const float height = known == heights_.end() ? kNotMeasured : known->second;
  if (out_of_sight(height)) {
    ImGui::Dummy(ImVec2(0.0f, height - ImGui::GetStyle().ItemSpacing.y));
    return;
  }
  const float top = ImGui::GetCursorPosY();
  draw_entry(client, index);
  ImGui::Dummy(ImVec2(0.0f, kEntryGap));
  heights_[number] = ImGui::GetCursorPosY() - top;
}

// What was kept of entries the conversation let go.
void AgentPanel::forget_before(std::size_t first) {
  parsed_.erase(parsed_.begin(), parsed_.lower_bound(first));
  heights_.erase(heights_.begin(), heights_.lower_bound(first));
}

// Text wraps anew at another width: the heights measured at the last one are no use.
void AgentPanel::forget_heights_if_resized() {
  const float width = ImGui::GetContentRegionAvail().x;
  if (width != measured_width_) heights_.clear();
  measured_width_ = width;
}

void AgentPanel::draw_entry(agent::AgentClient& client, std::size_t index) {
  const agent::Entry& entry = client.transcript().entries()[index];
  switch (entry.kind) {
    case EntryKind::Agent:
      return agent_view::draw_markdown(markdown(entry.number, entry.text));
    case EntryKind::Tool:
      return agent_view::draw_tool(entry.tool);
    case EntryKind::Plan:
      return agent_view::draw_plan(entry.plan);
    case EntryKind::Permission:
      return answer(client, entry.number, agent_view::draw_permission(entry.permission));
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
  const double now = ImGui::GetTime();
  if (stale(parsed, text, now)) parsed = {text.size(), now, agent::parse_markdown(text)};
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
  take_back_returned();
  if (std::exchange(focus_input_, false)) ImGui::SetKeyboardFocusHere();
  const float height = input_height();
  const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine;
  const bool entered = ImGui::InputTextMultiline(kPromptId, &input_, ImVec2(-1.0f, height), flags);
  if (shows_hint()) draw_hint(tr("editor.agent.input_hint"));
  if (!entered) return;
  send_input();
  focus_input_ = true;
}

void AgentPanel::send_input() {
  if (send(input_)) input_.clear();
}

// The box is empty and not being typed in: it shows what it is for.
bool AgentPanel::shows_hint() const { return input_.empty() && !ImGui::IsItemActive(); }

void AgentPanel::draw_actions() {
  if (icon_button(ICON_FA_PAPERCLIP, tr("editor.agent.attach"))) attach_selection();
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * editor_config::kIconButtonWidth);
  if (running()) {
    if (icon_button(ICON_FA_CIRCLE_STOP, tr("editor.agent.stop"))) stop();
    return;
  }
  ImGui::BeginDisabled(blank(input_));
  if (icon_button(ICON_FA_PAPER_PLANE, tr("editor.agent.send"))) send_input();
  ImGui::EndDisabled();
}

}  // namespace redgem::ui
