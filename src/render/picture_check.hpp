#pragma once

// A check of a picture read back from the graphics card. The pictures a scene is drawn into are
// opaque but for what a HUD draws over them (text, half-seen panels); one that comes back
// see-through over much of it was not drawn whole: the card dropped work (see frame_pacer.hpp).

#include <cstddef>
#include <cstdint>

namespace redgem {

inline constexpr std::size_t kRgbaChannels = 4;
inline constexpr std::size_t kAlphaChannel = 3;  // the channel of a pixel its opacity is in
inline constexpr std::uint8_t kOpaque = 255;

// How many of `pixels` RGBA pixels (8 bits a channel) are not fully opaque.
std::size_t see_through_pixels(const std::uint8_t* rgba, std::size_t pixels);

// More of a picture than a HUD covers: a picture with this share of it see-through was not drawn
// whole. (The dropped frames seen came back with 40% to 70% of them so.)
inline constexpr double kUnfinishedShare = 0.25;

// Whether a picture of `pixels` RGBA pixels is see-through over more than kUnfinishedShare of it.
bool unfinished(const std::uint8_t* rgba, std::size_t pixels);

}  // namespace redgem
