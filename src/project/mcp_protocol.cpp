#include "project/mcp_protocol.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

#include "core/config.hpp"
#include "core/short_text.hpp"

namespace redgem::tools::mcp {
namespace {
constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMissingMethod = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kInternalError = -32603;
constexpr int kUnsupportedVersion = -32022;
constexpr int kMaxJsonDepth = 16;
constexpr std::size_t kMaxResultBytes = 8192;  // the "8 KiB" the cut notice names
constexpr const char* kProtocolKey = "io.modelcontextprotocol/protocolVersion";
constexpr const char* kCapabilitiesKey = "io.modelcontextprotocol/clientCapabilities";
constexpr const char* kServerKey = "io.modelcontextprotocol/serverInfo";

struct Failure : std::runtime_error {
  Failure(int number, const std::string& text, Json details = nullptr)
      : std::runtime_error(text), code(number), data(std::move(details)) {}
  int code;
  Json data;
};

Json error(const Json& id, int code, const std::string& message, const Json& data = nullptr) {
  Json body{{"code", code}, {"message", message}};
  if (!data.is_null()) body["data"] = data;
  return {{"jsonrpc", "2.0"}, {"id", id}, {"error", body}};
}

// Handshake versions whose tool messages this server answers unchanged. A client asking for one
// gets it back; any other request is offered the newest, which the client may decline.
constexpr const char* kHandshakeVersions[] = {kLegacyVersion, "2025-06-18", "2025-03-26"};

std::string negotiated_version(const std::string& asked) {
  const auto* found = std::find(std::begin(kHandshakeVersions), std::end(kHandshakeVersions), asked);
  return found == std::end(kHandshakeVersions) ? kLegacyVersion : asked;
}

Json server_info() { return {{"name", "redgem"}, {"version", REDGEM_VERSION}}; }
Json capabilities() { return {{"tools", Json::object()}}; }

bool has_valid_id(const Json& message) {
  if (!message.contains("id")) return true;
  return message["id"].is_string() || message["id"].is_number_integer();
}

bool valid_envelope(const Json& message) {
  if (!message.is_object() || !has_valid_id(message)) return false;
  if (message.value("jsonrpc", Json()) != "2.0" || !message.value("method", Json()).is_string()) return false;
  return !message.contains("params") || message["params"].is_object();
}

bool modern_request(const Json& params) {
  if (!params.contains("_meta")) return false;
  if (!params["_meta"].is_object()) throw Failure(kInvalidParams, "_meta must be an object");
  return params["_meta"].contains(kProtocolKey);
}

void validate_modern(const Json& params) {
  const Json& meta = params["_meta"];
  if (!meta[kProtocolKey].is_string()) throw Failure(kInvalidParams, "protocolVersion must be a string");
  if (meta[kProtocolKey] != kModernVersion)
    throw Failure(kUnsupportedVersion, "Unsupported protocol version",
                  {{"supported", {kModernVersion}}, {"requested", meta[kProtocolKey]}});
  if (!meta.value(kCapabilitiesKey, Json()).is_object())
    throw Failure(kInvalidParams, "clientCapabilities must be an object");
}

Json discover() {
  return {{"supportedVersions", {kModernVersion}},
          {"capabilities", capabilities()},
          {"_meta", {{kServerKey, server_info()}}},
          {"ttlMs", 0},
          {"cacheScope", "private"}};
}

Json parse_message(const std::string& text) {
  const auto depth_guard = [](int depth, Json::parse_event_t, Json&) {
    if (depth > kMaxJsonDepth) throw Failure(kParseError, "JSON nesting limit exceeded");
    return true;
  };
  return Json::parse(text, depth_guard);
}

bool read_frame(std::istream& input, std::string& frame, bool& oversized) {
  frame.clear();
  oversized = false;
  char byte = 0;
  while (input.get(byte)) {
    if (byte == '\n') return true;
    if (frame.size() == kMaxRequestBytes) oversized = true;
    if (!oversized) frame.push_back(byte);
  }
  return oversized || !frame.empty();
}

void answer_frame(const std::string& frame, bool oversized, const Send& send, Session& session) {
  try {
    if (oversized) throw Failure(kInvalidRequest, "request exceeds 64 KiB");
    if (const auto reply = session.receive(parse_message(frame)))
      send(reply->dump(-1, ' ', false, Json::error_handler_t::replace));
  } catch (const Failure& problem) {
    send(error(nullptr, problem.code, problem.what()).dump());
  } catch (const Json::exception&) {
    send(error(nullptr, kParseError, "Invalid JSON").dump());
  }
}
}  // namespace

Session::Session(Json tools, Invoke invoke) : tools_(std::move(tools)), invoke_(std::move(invoke)) {}

Json Session::initialize(const Json& params) {
  if (initialized_) throw Failure(kInvalidRequest, "already initialized");
  if (!params.value("protocolVersion", Json()).is_string()) throw Failure(kInvalidParams, "protocolVersion required");
  if (!params.value("capabilities", Json()).is_object()) throw Failure(kInvalidParams, "capabilities required");
  initialized_ = true;
  return {{"protocolVersion", negotiated_version(params["protocolVersion"])},
          {"capabilities", capabilities()},
          {"serverInfo", server_info()}};
}

Json Session::call(const Json& params) {
  if (!params.value("name", Json()).is_string()) throw Failure(kInvalidParams, "tool name required");
  const std::string name = params["name"];
  const auto found = std::find_if(tools_.begin(), tools_.end(), [&](const Json& tool) { return tool["name"] == name; });
  if (found == tools_.end()) throw Failure(kInvalidParams, "unknown or disabled tool");
  const Json args = params.value("arguments", Json::object());
  if (!args.is_object()) throw Failure(kInvalidParams, "arguments must be an object");
  try {
    return invoke_(name, args);
  } catch (const std::invalid_argument& problem) {
    // A tool error, not a protocol error: the model sees it and can correct the call.
    return tool_result(problem.what(), true);
  }
}

Json Session::dispatch(const std::string& method, const Json& params, bool modern) {
  if (method == "ping") return Json::object();
  if (method == "initialize" && !modern) return initialize(params);
  if (!modern && !ready_) throw Failure(kInvalidRequest, "initialize first");
  if (method == "server/discover" && modern) return discover();
  return tool_method(method, params);
}

Json Session::tool_method(const std::string& method, const Json& params) {
  if (method == "tools/call") return call(params);
  if (method != "tools/list") throw Failure(kMissingMethod, "Method not found");
  if (params.contains("cursor")) throw Failure(kInvalidParams, "this tool list has no continuation cursor");
  return {{"tools", tools_}};
}

Json Session::result_for(const std::string& method, const Json& params) {
  const bool modern = modern_request(params);
  if (modern) validate_modern(params);
  Json result = dispatch(method, params, modern);
  if (modern) result["resultType"] = "complete";
  return result;
}

std::optional<Json> Session::receive(const Json& message) {
  if (!valid_envelope(message)) return error(nullptr, kInvalidRequest, "Invalid Request");
  const std::string method = message["method"];
  if (!message.contains("id")) {
    if (method == "notifications/initialized" && initialized_) ready_ = true;
    return std::nullopt;
  }
  return answer(message);
}

Json Session::answer(const Json& message) {
  const Json& id = message["id"];
  try {
    return Json{{"jsonrpc", "2.0"},
                {"id", id},
                {"result", result_for(message["method"], message.value("params", Json::object()))}};
  } catch (const Failure& problem) {
    return error(id, problem.code, problem.what(), problem.data);
  } catch (const std::exception&) {
    return error(id, kInternalError, "Internal error");
  }
}

void serve(std::istream& input, const Send& send, Session& session) {
  std::string frame;
  bool oversized = false;
  while (read_frame(input, frame, oversized)) answer_frame(frame, oversized, send, session);
}

Json tool_result(const std::string& text, bool failed) {
  std::string bounded(utf8_prefix(text, kMaxResultBytes));
  if (bounded.size() < text.size()) bounded += "\n[output cut at 8 KiB; narrow the request]";
  return {{"content", {{{"type", "text"}, {"text", bounded}}}}, {"isError", failed}};
}
}  // namespace redgem::tools::mcp
