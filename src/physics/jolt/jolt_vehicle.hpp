#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>

#include <vector>

#include "physics/physics_world.hpp"

namespace redgem::jolt {

// Which wheels share an axle, deduced from where the Blender wheels sit on the chassis.
struct AxleLayout {
  std::vector<int> front_wheels;  // wheel indices, forward is -Z
  std::vector<int> rear_wheels;
  // Left/right pairs for the differentials (-1 when a side has no wheel).
  struct Axle {
    int left = -1;
    int right = -1;
  };
  std::vector<Axle> axles;
};

AxleLayout layout_axles(const std::vector<WheelDesc>& wheels);

// Four-wheel-drive car: front wheels steer, rear wheels have the hand brake.
JPH::Ref<JPH::VehicleConstraint> build_vehicle(JPH::Body& chassis, const std::vector<WheelDesc>& wheels,
                                               JPH::ObjectLayer layer);

// Stills a vehicle's wheels, engine and controls. They keep their own spin: a teleported car must
// not drive off by itself.
void stop_vehicle(JPH::VehicleConstraint& vehicle);

}  // namespace redgem::jolt
