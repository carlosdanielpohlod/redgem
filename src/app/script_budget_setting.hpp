#pragma once

#include <chrono>
#include <filesystem>

namespace redgem {

// How long one call into the game's scripts may run (see script/script_budget.hpp), as game.json
// says under "script_budget_ms": a whole number of milliseconds, up to a minute. Without one, or
// with one that cannot be used (which is warned of), the engine's default.
//   "script_budget_ms": 2000
std::chrono::milliseconds script_budget_for(const std::filesystem::path& game_dir);

}  // namespace redgem
