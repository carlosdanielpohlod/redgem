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

// How long one call from the engine into a game's scripts may run (see script/script_budget.hpp),
// unless game.json says otherwise under kScriptBudgetKey: long enough for a scene script that
// builds a big level on a slow phone, short enough that a script stuck in a loop is a hitch
// rather than a frozen game.
inline constexpr int kDefaultScriptBudgetMs = 1000;
// The most a game may give its scripts: a minute, past which "never returns" is what it means.
inline constexpr int kMaxScriptBudgetMs = 60 * 1000;
inline constexpr const char* kScriptBudgetKey = "script_budget_ms";
// How often the watchdog looks at the scripts that run: a call is stopped at most two of these
// past its budget.
inline constexpr int kScriptWatchIntervalMs = 10;

}  // namespace redgem::config
