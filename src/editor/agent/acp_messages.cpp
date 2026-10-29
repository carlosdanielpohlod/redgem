#include "editor/agent/acp_messages.hpp"

#include <map>
#include <utility>

namespace redgem::agent {
namespace {

constexpr const char* kClientName = "redgem-editor";
constexpr const char* kClientTitle = "RedGem editor";
constexpr const char* kTextType = "text";
constexpr const char* kDiffType = "diff";
constexpr const char* kContentType = "content";
constexpr const char* kResourceLinkType = "resource_link";
constexpr const char* kResourceType = "resource";
constexpr const char* kSelectedOutcome = "selected";
constexpr const char* kCancelledOutcome = "cancelled";
constexpr const char* kImagePlaceholder = "[image]";  // what the transcript shows for a picture
constexpr const char* kTerminalPlaceholder = "[terminal output]";

std::string text_or_empty(const Json& value, const char* key) {
  const auto found = value.find(key);
  return found != value.end() && found->is_string() ? found->get<std::string>() : std::string();
}

bool is_type(const Json& block, const char* type) { return block.is_object() && text_or_empty(block, "type") == type; }

// A content block as text: text itself, the address of a link, the text of an embedded resource.
std::string block_text(const Json& block) {
  if (is_type(block, kTextType)) return text_or_empty(block, "text");
  if (is_type(block, kResourceLinkType)) return text_or_empty(block, "uri");
  if (is_type(block, kResourceType)) return text_or_empty(block.value("resource", Json::object()), "text");
  return block.is_object() && block.contains("type") ? kImagePlaceholder : "";
}

FileChange change_of(const Json& diff) {
  FileChange change;
  change.path = text_or_empty(diff, "path");
  change.new_text = text_or_empty(diff, "newText");
  if (diff.contains("oldText") && diff["oldText"].is_string()) change.old_text = diff["oldText"].get<std::string>();
  return change;
}

ToolContent tool_content(const Json& item) {
  ToolContent content;
  if (is_type(item, kDiffType)) {
    content.change = change_of(item);
  } else if (is_type(item, kContentType)) {
    content.text = block_text(item.value(kContentType, Json::object()));
  } else {
    content.text = kTerminalPlaceholder;
  }
  return content;
}

std::vector<ToolContent> tool_contents(const Json& items) {
  std::vector<ToolContent> contents;
  for (const Json& item : items) contents.push_back(tool_content(item));
  return contents;
}

std::vector<std::string> locations_of(const Json& items) {
  std::vector<std::string> locations;
  for (const Json& item : items) {
    if (item.is_object()) locations.push_back(text_or_empty(item, "path"));
  }
  return locations;
}

std::optional<ToolStatus> status_of(const Json& value) {
  static const std::map<std::string, ToolStatus> kStatuses = {{"pending", ToolStatus::Pending},
                                                              {"in_progress", ToolStatus::InProgress},
                                                              {"completed", ToolStatus::Completed},
                                                              {"failed", ToolStatus::Failed}};
  const auto found = value.is_string() ? kStatuses.find(value.get<std::string>()) : kStatuses.end();
  if (found == kStatuses.end()) return std::nullopt;
  return found->second;
}

PlanStatus plan_status_of(const std::string& value) {
  if (value == "completed") return PlanStatus::Completed;
  return value == "in_progress" ? PlanStatus::InProgress : PlanStatus::Pending;
}

std::vector<PlanItem> plan_of(const Json& update) {
  std::vector<PlanItem> plan;
  for (const Json& entry : update.value("entries", Json::array())) {
    if (entry.is_object())
      plan.push_back({text_or_empty(entry, "content"), plan_status_of(text_or_empty(entry, "status"))});
  }
  return plan;
}

std::optional<std::string> optional_text(const Json& update, const char* key) {
  if (!update.contains(key) || !update[key].is_string()) return std::nullopt;
  return update[key].get<std::string>();
}

bool has_list(const Json& update, const char* key) { return update.contains(key) && update[key].is_array(); }

ToolCall started_call(const Json& update) {
  ToolCall call;
  call.id = text_or_empty(update, "toolCallId");
  call.title = text_or_empty(update, "title");
  call.kind = text_or_empty(update, "kind");
  call.status = status_of(update.value("status", Json())).value_or(ToolStatus::Pending);
  call.content = tool_contents(update.value("content", Json::array()));
  call.locations = locations_of(update.value("locations", Json::array()));
  if (update.contains("rawInput")) call.input = compact(update["rawInput"]);
  return call;
}

PermissionKind permission_kind_of(const std::string& value) {
  if (value == "allow_always") return PermissionKind::AllowAlways;
  if (value == "reject_once") return PermissionKind::RejectOnce;
  if (value == "reject_always") return PermissionKind::RejectAlways;
  return PermissionKind::AllowOnce;
}

using UpdateHandler = bool (*)(Transcript&, const Json&);

bool message_chunk(Transcript& transcript, const Json& update) {
  transcript.append(EntryKind::Agent, block_text(update.value("content", Json::object())));
  return true;
}

bool thought_chunk(Transcript& transcript, const Json& update) {
  transcript.append(EntryKind::Thought, block_text(update.value("content", Json::object())));
  return true;
}

bool tool_call(Transcript& transcript, const Json& update) {
  transcript.start_tool(started_call(update));
  return true;
}

bool tool_call_update(Transcript& transcript, const Json& update) {
  transcript.update_tool(tool_patch(update));
  return true;
}

bool plan(Transcript& transcript, const Json& update) {
  transcript.set_plan(plan_of(update));
  return true;
}

UpdateHandler handler_for(const std::string& kind) {
  static const std::map<std::string, UpdateHandler> kHandlers = {{"agent_message_chunk", message_chunk},
                                                                 {"agent_thought_chunk", thought_chunk},
                                                                 {"tool_call", tool_call},
                                                                 {"tool_call_update", tool_call_update},
                                                                 {"plan", plan}};
  const auto found = kHandlers.find(kind);
  return found == kHandlers.end() ? nullptr : found->second;
}

}  // namespace

Json initialize_params(const std::string& client_version) {
  return {{"protocolVersion", kProtocolVersion},
          {"clientCapabilities", {{"fs", {{"readTextFile", true}, {"writeTextFile", true}}}, {"terminal", false}}},
          {"clientInfo", {{"name", kClientName}, {"title", kClientTitle}, {"version", client_version}}}};
}

Json new_session_params(const std::filesystem::path& folder, const std::vector<McpServer>& servers) {
  Json listed = Json::array();
  for (const McpServer& server : servers) {
    Json environment = Json::array();
    for (const EnvironmentVariable& variable : server.environment) {
      environment.push_back({{"name", variable.name}, {"value", variable.value}});
    }
    listed.push_back(
        {{"name", server.name}, {"command", server.command}, {"args", server.arguments}, {"env", environment}});
  }
  return {{"cwd", folder.u8string()}, {"mcpServers", listed}};
}

Json prompt_params(const std::string& session, const std::string& text, const std::vector<std::string>& context) {
  Json blocks = Json::array();
  for (const std::string& part : context) blocks.push_back({{"type", kTextType}, {"text", part}});
  blocks.push_back({{"type", kTextType}, {"text", text}});
  return {{"sessionId", session}, {"prompt", blocks}};
}

Json cancel_params(const std::string& session) { return {{"sessionId", session}}; }

AgentInfo agent_info(const Json& result) {
  AgentInfo info;
  info.protocol_version = result.value("protocolVersion", 0);
  const Json about = result.value("agentInfo", Json::object());
  info.name = text_or_empty(about, "title");
  if (info.name.empty()) info.name = text_or_empty(about, "name");
  for (const Json& login : result.value("authMethods", Json::array())) {
    if (login.is_object()) info.login_methods.push_back(text_or_empty(login, "name"));
  }
  return info;
}

bool apply_update(Transcript& transcript, const Json& params) {
  const Json update = params.value("update", Json::object());
  if (!update.is_object()) return false;
  const UpdateHandler handler = handler_for(text_or_empty(update, "sessionUpdate"));
  return handler != nullptr && handler(transcript, update);
}

ToolCallPatch tool_patch(const Json& update) {
  ToolCallPatch patch;
  patch.id = text_or_empty(update, "toolCallId");
  patch.title = optional_text(update, "title");
  patch.kind = optional_text(update, "kind");
  patch.status = status_of(update.value("status", Json()));
  if (has_list(update, "content")) patch.content = tool_contents(update["content"]);
  if (has_list(update, "locations")) patch.locations = locations_of(update["locations"]);
  if (update.contains("rawInput")) patch.input = compact(update["rawInput"]);
  return patch;
}

PermissionRequest permission_request(const Json& id, const Json& params) {
  std::vector<PermissionOption> options;
  for (const Json& option : params.value("options", Json::array())) {
    if (!option.is_object()) continue;
    options.push_back({text_or_empty(option, "optionId"), text_or_empty(option, "name"),
                       permission_kind_of(text_or_empty(option, "kind"))});
  }
  return {id, tool_patch(params.value("toolCall", Json::object())), std::move(options)};
}

Json permission_outcome(const PermissionRequest& request) {
  const PermissionOption* chosen = request.chosen();
  if (chosen == nullptr) return {{"outcome", {{"outcome", kCancelledOutcome}}}};
  return {{"outcome", {{"outcome", kSelectedOutcome}, {"optionId", chosen->id}}}};
}

std::string stop_reason(const Json& result) { return text_or_empty(result, "stopReason"); }

}  // namespace redgem::agent
