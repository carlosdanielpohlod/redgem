#pragma once

#include <functional>
#include <istream>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace redgem::tools::mcp {
using Json = nlohmann::json;
using Invoke = std::function<Json(const std::string&, const Json&)>;
using Send = std::function<void(const std::string&)>;
inline constexpr std::size_t kMaxRequestBytes = 64 * 1024;
inline constexpr const char* kModernVersion = "2026-07-28";
inline constexpr const char* kLegacyVersion = "2025-11-25";

// Modern requests carry their own metadata. Only legacy initialization is connection state.
class Session {
 public:
  Session(Json tools, Invoke invoke);
  std::optional<Json> receive(const Json& message);

 private:
  Json dispatch(const std::string& method, const Json& params, bool modern);
  Json tool_method(const std::string& method, const Json& params);
  Json result_for(const std::string& method, const Json& params);
  Json answer(const Json& message);
  Json initialize(const Json& params);
  Json call(const Json& params);
  Json tools_;
  Invoke invoke_;
  bool initialized_ = false;
  bool ready_ = false;
};

// Newline-delimited UTF-8 JSON. Oversize/deep messages fail without losing the next frame.
void serve(std::istream& input, const Send& send, Session& session);
Json tool_result(const std::string& text, bool failed = false);
}  // namespace redgem::tools::mcp
