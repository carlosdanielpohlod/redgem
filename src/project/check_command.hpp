#pragma once

// `redgem check <game> [--run-tests] [--all]`: whether a game is in order, said short. Every script
// compiles; every scene reads; every script a scene attaches is there and defines its class; every
// picture a scene names is there. With --run-tests, the game's own tests run too
// (tests/*_test.rb, with redgem-test). A line per problem ("error scripts/coin.rb:12: ..."), at most a screenful
// unless --all, then one line of summary. It ends with an error when anything is wrong.

#include <string>
#include <vector>

#include "project/tool_line.hpp"

namespace redgem::tools {

inline constexpr const char* kCheckCommand = "check";

ToolResult run_check_command(const std::vector<std::string>& args);

}  // namespace redgem::tools
