#pragma once

#include <string>

#include "app/scene_bodies.hpp"
#include "physics/physics_world.hpp"
#include "script/script_context.hpp"

// The joints a scene's objects come with (their `joint` properties, the editor's Joint card), made
// once the bodies they hold are in the physics world, as a script's Physics.joint makes them.
namespace redgem::joints {

// The object at the other end of an object's joint (`other`, a stable id): for an object spawned
// as part of a copy, the copy of that object in the same copy if there is one. kNoObject when no
// object has that id.
ObjectId partner_of(const Scene& scene, ObjectId id, const std::string& other);

// Makes the joint of every object that has one, into `made` (cleared first).
void create_all(const bodies::Source& source, PhysicsWorld& physics, SceneJoints& made);
// The same for one object; what is wrong with its properties is logged.
void create(const bodies::Source& source, PhysicsWorld& physics, ObjectId id, SceneJoints& made);

}  // namespace redgem::joints
