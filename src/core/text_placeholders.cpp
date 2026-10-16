// Filling the placeholders of a text: "Created {game}" with {"game", "Racer"}.

#include "core/texts.hpp"

namespace redgem {
namespace {

constexpr char kPlaceholderStart = '{';
constexpr char kPlaceholderEnd = '}';

// The value given for the placeholder `name`; nullptr when none was.
const std::string* value_of(const TextValues& values, std::string_view name) {
  for (const TextValue& value : values) {
    if (value.name == name) return &value.value;
  }
  return nullptr;
}

}  // namespace

std::string fill_placeholders(std::string_view text, const TextValues& values) {
  std::string filled;
  std::size_t from = 0;
  while (from < text.size()) {
    const std::size_t start = text.find(kPlaceholderStart, from);
    const std::size_t end = text.find(kPlaceholderEnd, start);
    if (end == std::string_view::npos) break;
    filled.append(text.substr(from, start - from));
    const std::string* value = value_of(values, text.substr(start + 1, end - start - 1));
    filled.append(value != nullptr ? std::string_view(*value) : text.substr(start, end - start + 1));
    from = end + 1;
  }
  filled.append(text.substr(from));
  return filled;
}

}  // namespace redgem
