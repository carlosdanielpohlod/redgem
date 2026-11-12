#include <doctest/doctest.h>

#include "core/config.hpp"
#include "physics/physics_config.hpp"
#include "physics/jolt/jolt_vehicle.hpp"
#include "physics/physics_world.hpp"

using namespace redgem;

namespace {

constexpr float kDt = config::kFixedDeltaSeconds;
constexpr int kSettleSteps = 120;
constexpr int kDriveSteps = 180;
constexpr float kMinCruiseSpeed = 3.0f;
constexpr float kMinDriveDistance = 3.0f;
constexpr float kMinGroundClearance = 0.15f;
constexpr std::size_t kWheelCount = 4;
constexpr int kOneStep = 1;
constexpr float kMinTurnDegrees = 15.0f;
constexpr float kWheelRadius = 0.4f;
constexpr float kWheelWidth = 0.3f;
constexpr float kAxleX = 0.9f;
constexpr float kAxleZ = 1.3f;
constexpr float kWheelY = -0.3f;
constexpr float kChassisHeight = 1.0f;
constexpr float kGroundHalfSize = 200.0f;
constexpr float kGroundHalfThickness = 0.5f;
constexpr Vec3 kChassisHalfExtents{0.9f, 0.3f, 2.0f};
constexpr VehicleInput kFullThrottle{1.0f, 0.0f, 0.0f, 0.0f};
constexpr VehicleInput kFullThrottleLeft{1.0f, -1.0f, 0.0f, 0.0f};
constexpr VehicleInput kAllBrakes{0.0f, 0.0f, 1.0f, 1.0f};
constexpr float kRestSpeed = 0.1f;
constexpr float kStoppedSpeed = 0.5f;
constexpr float kMaxSideDrift = 1.0f;
constexpr float kWheelHeightTolerance = 0.1f;
constexpr ObjectId kGround = 0;
constexpr ObjectId kCar = 1;
constexpr ObjectId kFirstWheel = 2;

Bounds box_bounds(Vec3 half) {
  Bounds bounds;
  bounds.include(-half);
  bounds.include(half);
  return bounds;
}

std::vector<WheelDesc> four_wheels() {
  std::vector<WheelDesc> wheels;
  ObjectId id = kFirstWheel;
  for (const float z : {-kAxleZ, kAxleZ}) {
    for (const float x : {-kAxleX, kAxleX}) {
      // (No scene here: the world only carries the handles back, so any serial does.)
      const ObjectHandle wheel{id, static_cast<ObjectSerial>(id)};
      ++id;
      wheels.push_back({wheel, {x, kWheelY, z}, kWheelRadius, kWheelWidth});
    }
  }
  return wheels;
}

std::unique_ptr<PhysicsWorld> world_with_car() {
  auto world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  BodyDesc ground;
  ground.object = kGround;
  ground.config = body_config_from_props({{"collider", std::string("box")}});
  ground.world.position = {0, -kGroundHalfThickness, 0};
  ground.bounds = box_bounds({kGroundHalfSize, kGroundHalfThickness, kGroundHalfSize});
  REQUIRE_FALSE(world->add_object(ground));

  BodyDesc car;
  car.object = kCar;
  car.config = body_config_from_props({{"body", std::string("vehicle")}});
  car.world.position = {0, kChassisHeight, 0};
  car.bounds = box_bounds(kChassisHalfExtents);
  car.wheels = four_wheels();
  REQUIRE_FALSE(world->add_object(car));
  return world;
}

void run(PhysicsWorld& world, int steps) {
  for (int i = 0; i < steps; ++i) world.step(kDt);
}

float heading_degrees(const Transform& pose) {
  const Vec3 forward = pose.forward();
  return radians_to_degrees(std::atan2(-forward.x, -forward.z));
}

}  // namespace

TEST_CASE("wheels are grouped into front and rear axles by position") {
  const jolt::AxleLayout layout = jolt::layout_axles(four_wheels());
  CHECK(layout.front_wheels == std::vector<int>{0, 1});
  CHECK(layout.rear_wheels == std::vector<int>{2, 3});
  REQUIRE(layout.axles.size() == 2);
  CHECK(layout.axles[0].left == 0);
  CHECK(layout.axles[0].right == 1);
  CHECK(layout.axles[1].left == 2);
  CHECK(layout.axles[1].right == 3);
}

TEST_CASE("vehicles default to a car-like mass") {
  CHECK(body_config_from_props({{"body", std::string("vehicle")}}).mass ==
        doctest::Approx(config::kVehicleDefaultMass));
}

TEST_CASE("a car settles on its wheels and drives forward") {
  auto world = world_with_car();
  run(*world, kSettleSteps);
  const Transform rest = *world->simulated_pose(kCar);
  CHECK(std::fabs(world->vehicle_forward_speed(kCar)) < kRestSpeed);
  CHECK(rest.position.y > kChassisHalfExtents.y + kMinGroundClearance);  // on its wheels, not its belly
  CHECK(world->wheel_poses(kCar).size() == kWheelCount);

  world->set_vehicle_input(kCar, kFullThrottle);
  run(*world, kDriveSteps);
  CHECK(world->vehicle_forward_speed(kCar) > kMinCruiseSpeed);
  const Transform moved = *world->simulated_pose(kCar);
  CHECK(moved.position.z < rest.position.z - kMinDriveDistance);  // forward is -Z
  CHECK(std::fabs(moved.position.x) < kMaxSideDrift);

  const std::vector<WheelPose> wheels = world->wheel_poses(kCar);
  CHECK(wheels.front().object.id == kFirstWheel);
  CHECK(std::fabs(wheels.front().world.position.y - kWheelRadius) < kWheelHeightTolerance);
}

TEST_CASE("steering turns the car and the brake stops it") {
  auto world = world_with_car();
  run(*world, kSettleSteps);
  world->set_vehicle_input(kCar, kFullThrottleLeft);
  run(*world, kDriveSteps);
  CHECK(heading_degrees(*world->simulated_pose(kCar)) > kMinTurnDegrees);

  world->set_vehicle_input(kCar, kAllBrakes);
  run(*world, kDriveSteps);
  CHECK(std::fabs(world->vehicle_forward_speed(kCar)) < kStoppedSpeed);
}

TEST_CASE("removing a vehicle removes its constraint") {
  auto world = world_with_car();
  run(*world, kOneStep);
  world->remove_object(kCar);
  CHECK_FALSE(world->has_object(kCar));
  CHECK(world->wheel_poses(kCar).empty());
  run(*world, kOneStep);
}
