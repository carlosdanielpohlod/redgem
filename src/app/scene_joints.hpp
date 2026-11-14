#pragma once

#include <utility>
#include <vector>

#include "app/scene_bodies.hpp"
#include "physics/physics_world.hpp"
#include "script/script_context.hpp"

// The joints a scene's objects come with (their `joint` properties, the editor's Joint card), made
// once the bodies they hold are in the physics world, as a script's Physics.joint makes them.
namespace redgem::joints {

// (original, its copy), as Scene::clone_pairs gives them.
using CopyPairs = std::vector<std::pair<ObjectId, ObjectId>>;

// Makes the joint of every object of a loaded scene that has one, into `made` (cleared first).
// The other end of each is the object whose stable id the joint names.
void create_all(const bodies::Source& source, PhysicsWorld& physics, SceneJoints& made);
// The same for the copies a spawn just made (once their bodies are there). A copy's joint holds
// to what its original's joint held to, or to the copy of it when that was copied too.
void create_copies(const bodies::Source& source, PhysicsWorld& physics, const CopyPairs& copies, SceneJoints& made);

}  // namespace redgem::joints
