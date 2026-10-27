#include "core/short_text.hpp"

#include <algorithm>

namespace redgem {
namespace {

constexpr unsigned char kContinuationMask = 0xC0;
constexpr unsigned char kContinuationByte = 0x80;

bool continuation(char byte) { return (static_cast<unsigned char>(byte) & kContinuationMask) == kContinuationByte; }

}  // namespace

std::string_view utf8_prefix(std::string_view text, std::size_t bytes) {
  std::size_t end = std::min(text.size(), bytes);
  while (end > 0 && end < text.size() && continuation(text[end])) --end;
  return text.substr(0, end);
}

}  // namespace redgem
