#pragma once

// MCP's `project` tool: what a game is beyond its scenes, read and changed as files.
//
//   settings / set_settings   game.json (set_settings merges `changes` into it, null taking a key away)
//   texts / set_texts         locales/<language>.json, the texts players read (I18n)
//   story / set_story         dialogues/, quests/ and items/ files, by `kind` and `name`
//   scenes / new_scene        the game's scenes, and a new scenes/<scene>.glb
//   new_game                  a new game next to this one, from a template
//
// Every write is checked as `redgem check` checks the game, and refused (the file left as it was)
// when it brings a problem the game did not have.

#include "project/mcp_tools.hpp"

namespace redgem::tools::mcp {

Json project_tool(const Options& options, const Json& args);

// The actions that only read, which --read-only keeps.
bool reads_project(const std::string& action);

}  // namespace redgem::tools::mcp
