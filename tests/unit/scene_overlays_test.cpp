#include <doctest/doctest.h>

#include "editor/scene_overlays.hpp"
#include "physics/body_config.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

namespace {

constexpr Vec3 kCrateMin{-2.0f, 0.0f, -1.0f};
constexpr Vec3 kCrateMax{2.0f, 3.0f, 1.0f};

SceneDesc scene_with_a_crate() {
  SceneDesc desc;
  MeshDesc crate;
  crate.bounds = Bounds{kCrateMin, kCrateMax, true};
  desc.meshes.push_back(crate);
  return desc;
}

}  // namespace

TEST_CASE("an object is boxed by its mesh, and an object without a mesh by a small box") {
  const SceneDesc desc = scene_with_a_crate();
  GameObject crate;
  crate.mesh = 0;
  const Bounds boxed = bounds_of(crate, desc);
  CHECK(approx_equal(boxed.min, kCrateMin, kTolerance));
  CHECK(approx_equal(boxed.max, kCrateMax, kTolerance));
  GameObject empty;
  const Bounds small = bounds_of(empty, desc);
  CHECK(approx_equal(small.min, default_collider_bounds().min, kTolerance));
  CHECK(approx_equal(small.max, default_collider_bounds().max, kTolerance));
}

TEST_CASE("the viewport colors come out as raylib takes them") {
  constexpr unsigned char kOrange[] = {255, 160, 0, 128};
  const ::Color color = rgba(kOrange);
  CHECK(color.r == kOrange[0]);
  CHECK(color.g == kOrange[1]);
  CHECK(color.b == kOrange[2]);
  CHECK(color.a == kOrange[3]);
}
