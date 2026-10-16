#include "storage/saved_values.hpp"

#include <cmath>
#include <cstdint>
#include <string>

namespace redgem::saved {
namespace {

constexpr double kLargestWhole = 9007199254740992.0;  // 2^53: whole numbers a double holds exactly

bool is_whole(double number) { return std::fabs(number) <= kLargestWhole && std::floor(number) == number; }

bool is_whole_float(const nlohmann::json& value) { return value.is_number_float() && is_whole(value.get<double>()); }

std::optional<nlohmann::json> kept_at(const nlohmann::json& value, int depth);

// An array or a hash, each of its members as it is kept.
std::optional<nlohmann::json> kept_members(const nlohmann::json& value, int depth) {
  if (depth >= kMaxDepth) return std::nullopt;
  nlohmann::json members = value;
  for (nlohmann::json& member : members) {
    const auto inner = kept_at(member, depth + 1);
    if (!inner) return std::nullopt;
    member = *inner;
  }
  return members;
}

std::optional<nlohmann::json> kept_at(const nlohmann::json& value, int depth) {
  if (value.is_structured()) return kept_members(value, depth);
  if (is_whole_float(value)) return nlohmann::json(value.get<std::int64_t>());
  return value;
}

}  // namespace

std::optional<nlohmann::json> kept(const nlohmann::json& value) { return kept_at(value, 0); }

std::optional<nlohmann::json> entry(const nlohmann::json& value) {
  if (value.is_null()) return std::nullopt;
  return kept(value);
}

std::string written(const nlohmann::json& value, int indent) {
  return value.dump(indent, ' ', false, nlohmann::json::error_handler_t::replace);
}

}  // namespace redgem::saved
