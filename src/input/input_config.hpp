#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "input/input_map.hpp"

// Actions and axes written down: the "input" section of a game's game.json, and the bindings a
// player saved, which are written the same way:
//   {"actions": {"jump": ["key:space", "pad:a"]},
//    "axes": {"steer": {"negative": ["key:a"], "positive": ["key:d"], "pad_axis": "left_x", "invert": false}}}
namespace redgem {

// What becomes of an action or axis with something wrong in it: kept with what could be read
// (a game's declaration: the game plays with the rest), or dropped whole (a player's saved
// change: the game's own binding applies instead of a part of the player's).
enum class BrokenEntries { Kept, Dropped };

// What a section declares. What is not understood (a binding with no such key or button, an
// entry that is not a list) is left out, and each such thing said in `problems`.
InputMap input_map_from_json(const nlohmann::json& section, std::vector<std::string>& problems,
                             BrokenEntries broken = BrokenEntries::Kept);

// And back: what input_map_from_json reads as the same map.
nlohmann::json input_map_json(const InputMap& map);

}  // namespace redgem
