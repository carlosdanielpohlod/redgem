#include "editor/agent/json_rpc.hpp"

namespace redgem::agent {
namespace {

constexpr const char* kVersion = "2.0";
constexpr char kLineEnd = '\n';
constexpr char kCarriageReturn = '\r';
constexpr int kOnOneLine = -1;          // dump(): no indentation, no line breaks
constexpr char kIndentCharacter = ' ';  // (unused on one line)

bool has_id(const Json& message) { return message.contains("id") && !message["id"].is_null(); }
bool has_method(const Json& message) { return message.contains("method") && message["method"].is_string(); }
bool is_response(const Json& message) {
  return has_id(message) && (message.contains("result") || message.contains("error"));
}
bool ends_with_return(const std::string& line) { return !line.empty() && line.back() == kCarriageReturn; }

RpcError error_of(const Json& error) {
  RpcError parsed;
  if (!error.is_object()) return parsed;
  const Json code = error.value("code", Json());
  const Json message = error.value("message", Json());
  if (code.is_number_integer()) parsed.code = code.get<int>();
  if (message.is_string()) parsed.message = message.get<std::string>();
  return parsed;
}

RpcMessage call_of(const Json& message) {
  RpcMessage parsed;
  parsed.kind = has_id(message) ? RpcKind::Request : RpcKind::Notification;
  parsed.method = message["method"].get<std::string>();
  parsed.params = message.contains("params") ? message["params"] : Json::object();
  if (has_id(message)) parsed.id = message["id"];
  return parsed;
}

RpcMessage response_of(const Json& message) {
  RpcMessage parsed;
  parsed.kind = RpcKind::Response;
  parsed.id = message["id"];
  if (message.contains("error")) parsed.error = error_of(message["error"]);
  if (message.contains("result")) parsed.result = message["result"];
  return parsed;
}

// Thrown by the parser's callback at a value nested too deep.
struct TooDeep {};

}  // namespace

std::optional<Json> parse_message(const std::string& line) {
  const auto depth_guard = [](int depth, Json::parse_event_t, Json&) {
    if (depth >= kMaxMessageDepth) throw TooDeep{};  // (depth counts from 0)
    return true;
  };
  try {
    Json message = Json::parse(line, depth_guard, false);
    if (message.is_discarded()) return std::nullopt;
    return message;
  } catch (const TooDeep&) {
    return std::nullopt;
  }
}

void LineReader::feed(const char* bytes, std::size_t size) {
  if (overflowed_) return;
  pending_.append(bytes, size);
}

std::optional<std::string> LineReader::next() {
  const std::size_t end = pending_.find(kLineEnd, scanned_);
  if (end == std::string::npos) {
    scanned_ = pending_.size();
    overflowed_ = overflowed_ || pending_.size() > max_line_;
    return std::nullopt;
  }
  std::string line = pending_.substr(0, end);
  pending_.erase(0, end + 1);
  scanned_ = 0;
  if (ends_with_return(line)) line.pop_back();
  return line;
}

std::string compact(const Json& value) {
  return value.dump(kOnOneLine, kIndentCharacter, false, Json::error_handler_t::replace);
}

std::string to_line(const Json& message) { return compact(message) + kLineEnd; }

RpcMessage classify(const Json& message) {
  if (!message.is_object()) return {};
  if (has_method(message)) return call_of(message);
  if (is_response(message)) return response_of(message);
  return {};
}

Json rpc_request(int id, const std::string& method, const Json& params) {
  return {{"jsonrpc", kVersion}, {"id", id}, {"method", method}, {"params", params}};
}

Json rpc_notification(const std::string& method, const Json& params) {
  return {{"jsonrpc", kVersion}, {"method", method}, {"params", params}};
}

Json rpc_result(const Json& id, const Json& result) { return {{"jsonrpc", kVersion}, {"id", id}, {"result", result}}; }

Json rpc_error(const Json& id, int code, const std::string& message) {
  return {{"jsonrpc", kVersion}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

}  // namespace redgem::agent
