#include "editor/agent/agent_client.hpp"

#include <cstdint>
#include <limits>
#include <utility>

#include "core/translations.hpp"

namespace redgem::agent {
namespace {

constexpr const char* kEndTurn = "end_turn";
constexpr const char* kCancelled = "cancelled";

using RequestHandler = void (AgentClient::*)(const RpcMessage&);

bool ran_too_long(const std::string& reason) { return reason == "max_tokens" || reason == "max_turn_requests"; }

// What the transcript says when a turn ends other than by the agent finishing it.
const char* turn_end_text(const std::string& reason) {
  if (reason == kCancelled) return tr("editor.agent.turn_cancelled");
  if (reason == "refusal") return tr("editor.agent.turn_refused");
  if (ran_too_long(reason)) return tr("editor.agent.turn_too_long");
  return nullptr;
}

// A whole number of lines (JSON has no unsigned numbers: a parser may read 2 as signed).
bool is_count(const Json& value) {
  return value.is_number_integer() && value.get<std::int64_t>() >= 0 &&
         value.get<std::int64_t>() <= std::numeric_limits<std::uint32_t>::max();
}

std::optional<std::uint32_t> count_of(const Json& params, const char* key) {
  const Json value = params.value(key, Json());
  if (!is_count(value)) return std::nullopt;
  return value.get<std::uint32_t>();
}

LineRange range_of(const Json& params) { return {count_of(params, "line"), count_of(params, "limit")}; }

bool names_a_file(const Json& params) { return params.contains("path") && params["path"].is_string(); }

bool carries_text(const Json& params) {
  return names_a_file(params) && params.contains("content") && params["content"].is_string();
}

Json read_answer(const Workspace& workspace, const RpcMessage& message) {
  if (!names_a_file(message.params)) return rpc_error(message.id, kInvalidParams, "a path is needed");
  std::string error;
  const auto text = workspace.read(message.params["path"].get<std::string>(), range_of(message.params), error);
  if (!text) return rpc_error(message.id, kResourceNotFound, error);
  return rpc_result(message.id, {{"content", *text}});
}

Json write_answer(const Workspace& workspace, const RpcMessage& message) {
  if (!carries_text(message.params)) {
    return rpc_error(message.id, kInvalidParams, "a path and its content are needed");
  }
  std::string error;
  const std::string path = message.params["path"].get<std::string>();
  if (!workspace.write(path, message.params["content"].get<std::string>(), error)) {
    return rpc_error(message.id, kInternalError, error);
  }
  return rpc_result(message.id, Json::object());
}

}  // namespace

AgentClient::AgentClient(ClientSetup setup, Send send)
    : setup_(std::move(setup)), send_(std::move(send)), workspace_(setup_.folder) {}

void AgentClient::begin() {
  state_ = AgentState::Initializing;
  request(Call::Initialize, method::kInitialize, initialize_params(setup_.client_version));
}

void AgentClient::receive(const Json& message) {
  // (What an agent sends is checked field by field; a field of an unexpected type that slips past a
  // check is the agent's fault, and ends its conversation rather than the editor.)
  try {
    take(message);
  } catch (const std::exception& error) {
    fail(tr("editor.agent.malformed", {{"error", error.what()}}));
  }
}

void AgentClient::take(const Json& message) {
  const RpcMessage parsed = classify(message);
  switch (parsed.kind) {
    case RpcKind::Response:
      return on_response(parsed);
    case RpcKind::Request:
      return on_request(parsed);
    case RpcKind::Notification:
      return on_notification(parsed);
    case RpcKind::Invalid:
      return;
  }
}

bool AgentClient::prompt(const std::string& text, const std::vector<std::string>& context) {
  if (state_ != AgentState::Ready) return false;
  transcript_.add(EntryKind::User, text);
  state_ = AgentState::Prompting;
  request(Call::Prompt, method::kPrompt, prompt_params(session_, text, context));
  return true;
}

void AgentClient::cancel() {
  if (!busy()) return;
  cancelling_ = true;
  send_(rpc_notification(method::kCancel, cancel_params(session_)));
  cancel_questions();
}

// Every question still waiting is given up, and the agent told so (ACP wants an answer to each).
void AgentClient::cancel_questions() {
  for (const std::size_t entry : transcript_.waiting_permissions()) {
    PermissionRequest* request = transcript_.permission(entry);
    request->cancel();
    answer_permission(*request);
  }
}

bool AgentClient::choose(std::size_t entry, const std::string& option_id) {
  PermissionRequest* request = transcript_.permission(entry);
  if (request == nullptr || !request->choose(option_id)) return false;
  answer_permission(*request);
  return true;
}

void AgentClient::fail(const std::string& why) {
  if (state_ == AgentState::Failed) return;
  for (const std::size_t entry : transcript_.waiting_permissions()) transcript_.permission(entry)->cancel();
  state_ = AgentState::Failed;
  error_ = why;
  transcript_.add(EntryKind::Error, why);
}

int AgentClient::request(Call call, const char* method, const Json& params) {
  const int id = ++next_id_;
  waiting_[id] = call;
  send_(rpc_request(id, method, params));
  return id;
}

void AgentClient::on_response(const RpcMessage& message) {
  const auto found = message.id.is_number_integer() ? waiting_.find(message.id.get<int>()) : waiting_.end();
  if (found == waiting_.end()) return;
  const Call call = found->second;
  waiting_.erase(found);
  if (call == Call::Initialize) return on_initialized(message);
  if (call == Call::NewSession) return on_session(message);
  if (call == Call::LoadSession) return on_loaded(message);
  on_turn_end(message);
}

void AgentClient::on_initialized(const RpcMessage& message) {
  if (message.error) return fail(message.error->message);
  info_ = agent_info(message.result);
  if (info_.protocol_version != kProtocolVersion) {
    return fail(tr("editor.agent.protocol_mismatch", {{"version", std::to_string(info_.protocol_version)}}));
  }
  state_ = AgentState::OpeningSession;
  open_session();
}

// A past conversation when one was asked for and the agent can replay it; else a new one, saying why.
void AgentClient::open_session() {
  if (setup_.load_session.empty()) {
    request(Call::NewSession, method::kNewSession, new_session_params(workspace_.folder(), setup_.servers));
    return;
  }
  if (!info_.loads_sessions) return start_anew(tr("editor.agent.cannot_load"));
  request(Call::LoadSession, method::kLoadSession,
          load_session_params(setup_.load_session, workspace_.folder(), setup_.servers));
}

// A new conversation in place of the past one asked for, with why in the transcript.
void AgentClient::start_anew(const std::string& why) {
  transcript_.clear();  // (what was replayed of the past conversation is not this one's)
  transcript_.add(EntryKind::Notice, why);
  setup_.load_session.clear();
  open_session();
}

// The agent replayed the conversation (its updates came before this answer): it goes on from there.
// One it could not find starts anew.
void AgentClient::on_loaded(const RpcMessage& message) {
  if (message.error) return start_anew(tr("editor.agent.load_failed", {{"error", message.error->message}}));
  session_ = setup_.load_session;
  state_ = AgentState::Ready;
}

void AgentClient::on_session(const RpcMessage& message) {
  if (message.error) {
    needs_login_ = message.error->code == kAuthRequired;
    return fail(message.error->message);
  }
  session_ = field_text(message.result, "sessionId");
  if (session_.empty()) return fail(tr("editor.agent.malformed", {{"error", "session/new gave no sessionId"}}));
  state_ = AgentState::Ready;
}

void AgentClient::on_turn_end(const RpcMessage& message) {
  cancel_questions();
  cancelling_ = false;
  state_ = AgentState::Ready;
  if (message.error) return transcript_.add(EntryKind::Error, message.error->message);
  say_why_the_turn_ended(stop_reason(message.result));
}

void AgentClient::say_why_the_turn_ended(const std::string& reason) {
  if (reason == kEndTurn) return;
  if (const char* text = turn_end_text(reason)) transcript_.add(EntryKind::Notice, text);
}

void AgentClient::on_request(const RpcMessage& message) {
  static const std::map<std::string, RequestHandler> kHandlers = {
      {method::kRequestPermission, &AgentClient::ask_permission},
      {method::kReadTextFile, &AgentClient::read_file},
      {method::kWriteTextFile, &AgentClient::write_file}};
  const auto found = kHandlers.find(message.method);
  if (found == kHandlers.end()) return refuse(message.id, kMethodNotFound, "the editor has no " + message.method);
  (this->*(found->second))(message);
}

void AgentClient::on_notification(const RpcMessage& message) {
  if (message.method == method::kUpdate) apply_update(transcript_, message.params);
}

void AgentClient::read_file(const RpcMessage& message) { answer_file(message); }

void AgentClient::write_file(const RpcMessage& message) { answer_file(message); }

void AgentClient::answer_file(const RpcMessage& message) {
  if (const auto answer = answer_file_request(workspace_, message)) send_(*answer);
}

void AgentClient::ask_permission(const RpcMessage& message) {
  PermissionRequest request = permission_request(message.id, message.params);
  // (A question that crossed Stop on its way is not shown: the turn it belongs to is ending.)
  if (cancelling_) {
    request.cancel();
    return answer_permission(request);
  }
  transcript_.ask(request);
}

void AgentClient::answer_permission(PermissionRequest& request) {
  answer(request.request_id(), permission_outcome(request));
}

bool is_file_method(const std::string& name) { return name == method::kReadTextFile || name == method::kWriteTextFile; }

bool is_file_request(const RpcMessage& message) {
  return message.kind == RpcKind::Request && is_file_method(message.method);
}

std::optional<Json> answer_file_request(const Workspace& workspace, const RpcMessage& message) {
  if (!is_file_request(message)) return std::nullopt;
  if (message.method == method::kReadTextFile) return read_answer(workspace, message);
  return write_answer(workspace, message);
}

}  // namespace redgem::agent
