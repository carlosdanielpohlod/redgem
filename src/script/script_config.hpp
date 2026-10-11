#pragma once

// The numbers of the scripting layer.

namespace redgem::config {

// Width of a letter as a share of the text size, to estimate text widths when there is no font
// to measure with (headless runs).
inline constexpr float kEstimatedLetterWidth = 0.55f;

}  // namespace redgem::config
