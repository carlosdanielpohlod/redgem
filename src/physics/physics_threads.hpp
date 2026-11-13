#pragma once

// How many threads solve a game's physics: the "threads" key of game.json's "physics" section.
//   "physics": {"threads": 4}
// 1 (the default) solves every step on the thread that runs the game, as RedGem always did. More
// solve each step with a pool of that many threads (see physics/physics_config.hpp), and the game
// steps its world on a thread of its own while the frame before is drawn (see app/engine.hpp).

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace redgem {

inline constexpr const char* kPhysicsThreadsKey = "threads";

// The threads the section asks for; the default, and a warning, when it asks for what cannot be
// (anything but a whole number from 1 to config::kMaxPhysicsThreads).
int physics_threads_from_json(const nlohmann::json& physics, std::vector<std::string>& warnings);
// Puts `threads` in the section (an object), or takes the key out for the default.
void physics_threads_to_json(int threads, nlohmann::json& physics);

}  // namespace redgem
