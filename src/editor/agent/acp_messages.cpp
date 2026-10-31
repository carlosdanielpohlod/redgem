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

bool is_type(const Json& block, const char* type) { return block.is_object() && field_text(block, "type") == type; }

// A content block as text: text itself, the address of a link, the text of an embedded resource.
std::string block_text(const Json& block) {
  if (is_type(block, kTextType)) return field_text(block, "text");
  if (is_type(block, kResourceLinkType)) return field_text(block, "uri");
  if (is_type(block, kResourceType)) return field_text(field_object(block, "resource"), "text");
  return block.is_object() && block.contains("type") ? kImagePlaceholder : "";
}

FileChange change_of(const Json& diff) {
  FileChange change;
  change.path = field_text(diff, "path");
  change.lines = diff_lines(field_text(diff, "oldText"), field_text(diff, "newText"));
  return change;
}

ToolContent tool_content(const Json& item) {
  ToolContent content;
  if (is_type(item, kDiffType)) {
    content.change = change_of(item);
  } else if (is_type(item, kContentType)) {
    content.text = block_text(field_object(item, kContentType));
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
    if (item.is_object()) locations.push_back(field_text(item, "path"));
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
  for (const Json& entry : field_list(update, "entries")) {
    if (entry.is_object()) plan.push_back({field_text(entry, "content"), plan_status_of(field_text(entry, "status"))});
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
  call.id = field_text(update, "toolCallId");
  call.title = field_text(update, "title");
  call.kind = field_text(update, "kind");
  call.status = status_of(field(update, "status")).value_or(ToolStatus::Pending);
  call.content = tool_contents(field_list(update, "content"));
  call.locations = locations_of(field_list(update, "locations"));
  if (update.contains("rawInput")) call.input = compact(update["rawInput"]);
  return call;
}

PermissionKind permission_kind_of(const std::string& value) {
  static const std::map<std::string, PermissionKind> kKinds = {{"allow_once", PermissionKind::AllowOnce},
                                                               {"allow_always", PermissionKind::AllowAlways},
                                                               {"reject_once", PermissionKind::RejectOnce},
                                                               {"reject_always", PermissionKind::RejectAlways}};
  const auto found = kKinds.find(value);
  return found == kKinds.end() ? PermissionKind::Other : found->second;
}

using UpdateHandler = bool (*)(Transcript&, const Json&);

bool message_chunk(Transcript& transcript, const Json& update) {
  transcript.append(EntryKind::Agent, block_text(field_object(update, "content")));
  return true;
}

// What the user said, as an agent replays a past conversation (session/load).
bool user_chunk(Transcript& transcript, const Json& update) {
  transcript.append(EntryKind::User, block_text(field_object(update, "content")));
  return true;
}

bool thought_chunk(Transcript& transcript, const Json& update) {
  transcript.append(EntryKind::Thought, block_text(field_object(update, "content")));
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
  static const std::map<std::string, UpdateHandler> kHandlers = {
      {"user_message_chunk", user_chunk},     {"agent_message_chunk", message_chunk},
      {"agent_thought_chunk", thought_chunk}, {"tool_call", tool_call},
      {"tool_call_update", tool_call_update}, {"plan", plan}};
  const auto found = kHandlers.find(kind);
  return found == kHandlers.end() ? nullptr : found->second;
}

}  // namespace

Json field(const Json& value, const char* key) {
  if (!value.is_object()) return Json();
  const auto found = value.find(key);
  return found == value.end() ? Json() : *found;
}

Json field_object(const Json& value, const char* key) {
  Json found = field(value, key);
  return found.is_object() ? found : Json::object();
}

Json field_list(const Json& value, const char* key) {
  Json found = field(value, key);
  return found.is_array() ? found : Json::array();
}

std::string field_text(const Json& value, const char* key) {
  const Json found = field(value, key);
  return found.is_string() ? found.get<std::string>() : std::string();
}

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

Json load_session_params(const std::string& session, const std::filesystem::path& folder,
                         const std::vector<McpServer>& servers) {
  Json params = new_session_params(folder, servers);
  params["sessionId"] = session;
  return params;
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
  const Json version = field(result, "protocolVersion");
  info.protocol_version = version.is_number_integer() ? version.get<int>() : 0;
  const Json about = field_object(result, "agentInfo");
  info.name = field_text(about, "title");
  if (info.name.empty()) info.name = field_text(about, "name");
  info.loads_sessions = field(field_object(result, "agentCapabilities"), "loadSession") == true;
  for (const Json& login : field_list(result, "authMethods")) {
    if (login.is_object()) info.login_methods.push_back(field_text(login, "name"));
  }
  return info;
}

bool apply_update(Transcript& transcript, const Json& params) {
  const Json update = field_object(params, "update");
  if (!update.is_object()) return false;
  const UpdateHandler handler = handler_for(field_text(update, "sessionUpdate"));
  return handler != nullptr && handler(transcript, update);
}

ToolCallPatch tool_patch(const Json& update) {
  ToolCallPatch patch;
  patch.id = field_text(update, "toolCallId");
  patch.title = optional_text(update, "title");
  patch.kind = optional_text(update, "kind");
  patch.status = status_of(field(update, "status"));
  if (has_list(update, "content")) patch.content = tool_contents(update["content"]);
  if (has_list(update, "locations")) patch.locations = locations_of(update["locations"]);
  if (update.contains("rawInput")) patch.input = compact(update["rawInput"]);
  return patch;
}

PermissionRequest permission_request(const Json& id, const Json& params) {
  std::vector<PermissionOption> options;
  for (const Json& option : field_list(params, "options")) {
    if (!option.is_object()) continue;
    options.push_back(
        {field_text(option, "optionId"), field_text(option, "name"), permission_kind_of(field_text(option, "kind"))});
  }
  return {id, tool_patch(field_object(params, "toolCall")), std::move(options)};
}

Json permission_outcome(const PermissionRequest& request) {
  const PermissionOption* chosen = request.chosen();
  if (chosen == nullptr) return {{"outcome", {{"outcome", kCancelledOutcome}}}};
  return {{"outcome", {{"outcome", kSelectedOutcome}, {"optionId", chosen->id}}}};
}

std::string stop_reason(const Json& result) { return field_text(result, "stopReason"); }

}  // namespace redgem::agent
