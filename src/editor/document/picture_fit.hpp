#pragma once

// A picture put on a material, as its import settings' max_size (core/import_settings.hpp) says
// the scene should carry it.

#include <cstdint>
#include <optional>
#include <vector>

namespace redgem::picture_fit {

// The picture's bytes with no side longer than `max_size`: as they are when it already fits, a
// smaller copy written as PNG when it does not. nullopt when it is no PNG, JPEG or still WebP, or
// a bigger one cannot be decoded or written again.
std::optional<std::vector<std::uint8_t>> fitted_picture(const std::vector<std::uint8_t>& encoded, int max_size);

}  // namespace redgem::picture_fit
