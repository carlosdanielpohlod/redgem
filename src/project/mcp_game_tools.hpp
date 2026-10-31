#pragma once

// The MCP tools that play the game's saved files themselves: a small picture of a moment
// (game_shot) and questions to the game after some frames of play (game_query, see
// runtime_query.hpp). Both play the same way `redgem run` does: fixed steps, seed 0, nothing pressed.

#include "project/mcp_tools.hpp"

namespace redgem::tools::mcp {

// Frames game_query plays before it answers when the request does not say: one second.
inline constexpr int kDefaultQueryFrames = 60;
// game_shot's picture when the request does not say: 512 pixels wide, at one second.
inline constexpr int kDefaultShotWidth = 512;
inline constexpr double kDefaultShotSeconds = 1.0;
// Pictures have the shape of a common screen: 16 wide for 9 high.
inline constexpr int kShotAspectWidth = 16;
inline constexpr int kShotAspectHeight = 9;

Json game_shot(const Options& options, const Json& args);
Json game_query(const Options& options, const Json& args);

}  // namespace redgem::tools::mcp
