#pragma once

#include <cstddef>
#include <string_view>

namespace redgem {

// A prefix of valid UTF-8 that fits in `bytes`, ending before a partially cut character.
std::string_view utf8_prefix(std::string_view text, std::size_t bytes);

}  // namespace redgem
