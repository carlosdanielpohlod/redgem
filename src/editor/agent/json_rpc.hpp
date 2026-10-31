#pragma once

// JSON-RPC 2.0 as the Agent Client Protocol carries it over a program's standard streams: one
// message per line, a line never broken inside a message.

#include <cstddef>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace redgem::agent {

using Json = nlohmann::json;

// The error codes of JSON-RPC, and the one ACP adds for an agent that wants its user to log in.
inline constexpr int kParseError = -32700;
inline constexpr int kInvalidRequest = -32600;
inline constexpr int kMethodNotFound = -32601;
inline constexpr int kInvalidParams = -32602;
inline constexpr int kInternalError = -32603;
inline constexpr int kAuthRequired = -32000;
inline constexpr int kResourceNotFound = -32002;

// How deeply a message read from an agent may nest: far past what the protocol's messages need, and
// far short of what copying and freeing a value (which recurse) can take.
inline constexpr int kMaxMessageDepth = 64;

// The longest line read from an agent: a message holding a whole file's text and its change fits.
inline constexpr std::size_t kMaxLineBytes = std::size_t{16} * 1024 * 1024;

// Bytes as they come from a stream, cut into lines.
class LineReader {
 public:
  explicit LineReader(std::size_t max_line = kMaxLineBytes) : max_line_(max_line) {}
  void feed(const char* bytes, std::size_t size);
  // The next whole line, without its end ("\n" or "\r\n"); nullopt until one has come.
  std::optional<std::string> next();
  // A line went past the longest one taken: the stream cannot be read on.
  bool overflowed() const { return overflowed_; }

 private:
  std::string pending_;
  std::size_t scanned_ = 0;  // how much of pending_ is known to hold no line end
  std::size_t max_line_;
  bool overflowed_ = false;
};

// A value as JSON text on one line; text that is not UTF-8 is replaced rather than thrown about (it
// came from a file).
std::string compact(const Json& value);
// A message as one line to write (every line break inside strings is escaped).
std::string to_line(const Json& message);

// A line read from an agent as JSON; nullopt for one that is none, or nests deeper than kMaxMessageDepth.
std::optional<Json> parse_message(const std::string& line);

// What a message is.
enum class RpcKind { Request, Notification, Response, Invalid };
struct RpcError {
  int code = kInternalError;
  std::string message;
};
struct RpcMessage {
  RpcKind kind = RpcKind::Invalid;
  Json id;                        // requests and responses
  std::string method;             // requests and notifications
  Json params;                    // requests and notifications (an empty object when there were none)
  Json result;                    // a response that succeeded
  std::optional<RpcError> error;  // a response that failed
};
RpcMessage classify(const Json& message);

Json rpc_request(int id, const std::string& method, const Json& params);
Json rpc_notification(const std::string& method, const Json& params);
Json rpc_result(const Json& id, const Json& result);
Json rpc_error(const Json& id, int code, const std::string& message);

}  // namespace redgem::agent
