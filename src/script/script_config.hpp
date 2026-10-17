#pragma once

#include <cstddef>
#include <cstdint>

// The numbers of the scripting layer.

namespace redgem::config {

// Width of a letter as a share of the text size, to estimate text widths when there is no font
// to measure with (a game nobody draws).
inline constexpr float kEstimatedLetterWidth = 0.55f;

// A game's translation files (<game>/locales/<language>.json) are read whole when its scripts start:
// one larger than this is left out, so a game cannot make the engine hold a file of any size.
inline constexpr std::uintmax_t kMaxLocaleFileBytes = 1024 * 1024;
// The longest name a language goes by ("en", "pt_BR", "zh-Hant"): a file's or I18n.locale's.
inline constexpr std::size_t kMaxLanguageNameLength = 16;

}  // namespace redgem::config
