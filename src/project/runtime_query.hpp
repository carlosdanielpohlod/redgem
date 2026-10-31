#pragma once

// Questions to a game while it plays (MCP's game_query): the live state of one object, what a ray
// between two points hits and what a sphere overlaps. Each answer is a few short lines, numbers
// rounded as a scene list rounds them, saying only what differs from an object's defaults.

#include <cstddef>
#include <string>

#include "app/engine.hpp"
#include "project/mcp_protocol.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::tools {

// The actions game_query takes.
inline constexpr const char* kInspectAction = "inspect";
inline constexpr const char* kRaycastAction = "raycast";
inline constexpr const char* kOverlapAction = "overlap";
// The most hits a ray or an overlap lists; the rest are counted.
inline constexpr std::size_t kMostQueryHits = 10;

// The answer to `args` about the running `engine`, whose loaded objects are the nodes of `scene`
// (objects are named by #id, path or name as in a scene list; objects spawned while playing by
// name). Throws std::invalid_argument, saying what to change, for a request it cannot answer.
std::string answer_query(Engine& engine, const SceneDesc& scene, const mcp::Json& args);

// The point "x,y,z" of the argument `key`. Throws std::invalid_argument naming the argument.
Vec3 point_argument(const mcp::Json& args, const char* key);

}  // namespace redgem::tools
