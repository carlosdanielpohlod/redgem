#pragma once

#include <raylib.h>

#include <cstdint>
#include <set>
#include <vector>

namespace redgem {

// Decodes a bounded game-supplied face with FreeType, returning an owned raylib font or an empty one.
Font rasterize_game_font(const std::vector<std::uint8_t>& bytes, const std::set<int>& letters, int pixels);

}  // namespace redgem
