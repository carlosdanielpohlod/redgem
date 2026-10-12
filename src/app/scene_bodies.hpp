#pragma once

#include "physics/physics_world.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"
#include "script/script_context.hpp"

// What ties a scene's objects to their bodies in a physics world: which objects get one, and who
// follows whom each step.
namespace redgem::bodies {

// A scene as its bodies are made from it: the objects, and the meshes they are shaped by.
struct Source {
  const Scene& scene;
  const SceneDesc& desc;
};

// Gives a body to every object whose properties ask for one (see body_config_from_props).
void create_all(const Source& source, PhysicsWorld& physics);
// The same for one object; what is wrong with its properties is logged.
void create(const Source& source, PhysicsWorld& physics, ObjectId id);

// Before a step: bodies that follow their object (kinematic ones, triggers) are told where it is.
void push_poses(const Scene& scene, PhysicsWorld& physics);
// After a step: objects that follow their body are put where it went, a vehicle's wheels too.
void pull_poses(Scene& scene, PhysicsWorld& physics);

// A script moved or turned an object: its simulated body goes along.
void follow_write(const Scene& scene, PhysicsWorld& physics, ObjectId id, TransformWrite write);

}  // namespace redgem::bodies
