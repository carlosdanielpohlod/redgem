#pragma once

// A sky picture from a game's images folder, decoded to linear light: a Radiance picture (.hdr,
// light brighter than white) or an ordinary PNG or JPEG (sRGB, so never brighter than white:
// `sky_intensity` makes up for that). Its size is checked before it is decoded, as every picture's.

#include <filesystem>
#include <optional>
#include <string>

#include "core/float_image.hpp"

namespace redgem::render_detail {

// The picture `name` means in `folder` (as it is, or with .hdr, .png, .jpg or .jpeg), at most
// `widest` pixels wide. nullopt, said in a warning, when there is none or it cannot be decoded.
std::optional<FloatImage> load_sky_picture(const std::filesystem::path& folder, const std::string& name,
                                           std::uint32_t widest);

}  // namespace redgem::render_detail
