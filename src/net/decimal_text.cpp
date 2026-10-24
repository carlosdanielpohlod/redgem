#include "net/decimal_text.hpp"

#include <algorithm>
#include <cctype>

namespace redgem::net {

bool is_decimal(const std::string& text, std::size_t most_digits) {
  const bool digits_only =
      std::all_of(text.begin(), text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
  return digits_only && !text.empty() && text.size() <= most_digits;
}

}  // namespace redgem::net
