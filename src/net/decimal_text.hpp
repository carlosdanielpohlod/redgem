#pragma once

#include <cstddef>
#include <string>

namespace redgem::net {

// Whether a text is a whole number of at most `most_digits` digits (no sign, nothing else): how
// numbers that came from outside (a port, a length, an address's parts) are checked before use.
bool is_decimal(const std::string& text, std::size_t most_digits);

}  // namespace redgem::net
