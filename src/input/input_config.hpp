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

// What a section declares. What is not understood (a binding with no such key or button, an
// entry that is not a list) is left out, and each such thing said in `problems`.
InputMap input_map_from_json(const nlohmann::json& section, std::vector<std::string>& problems);

// And back: what input_map_from_json reads as the same map.
nlohmann::json input_map_json(const InputMap& map);

}  // namespace redgem
