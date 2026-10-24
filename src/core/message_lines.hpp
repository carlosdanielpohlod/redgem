#pragma once

// What a program says about a game, as one short line for whoever reads it in a terminal or a
// tool: a message's first line, led by the first place in the game's own files its backtrace
// names ("scripts/coin.rb:12: ..."), with the game's folder left out of every path.

#include <filesystem>
#include <string>

namespace redgem {

std::string compact_message(const std::string& message, const std::filesystem::path& game_dir);

}  // namespace redgem
