#include <doctest/doctest.h>

#include <vector>

#include "render/material_emission.hpp"

using namespace redgem;

namespace {

const Vec3 kWarmLight{3.0f, 2.2f, 1.2f};
const Vec3 kFileGlow{0.1f, 0.0f, 0.0f};

MaterialDesc named(const char* name, Vec3 emissive = Vec3::zero()) {
  MaterialDesc material;
  material.name = name;
  material.emissive = emissive;
  return material;
}

bool same(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

}  // namespace

TEST_CASE("with_emission lights every material of a name and leaves the others as the file has them") {
  const std::vector<MaterialDesc> loaded = {named("window_glass"), named("plaster", kFileGlow), named("window_glass")};
  const std::vector<MaterialDesc> drawn = with_emission(loaded, {{"window_glass", kWarmLight}});
  REQUIRE(drawn.size() == loaded.size());
  CHECK(same(drawn[0].emissive, kWarmLight));
  CHECK(same(drawn[2].emissive, kWarmLight));
  CHECK(same(drawn[1].emissive, kFileGlow));
  CHECK(same(loaded[0].emissive, Vec3::zero()));  // the file's own stay for a reset
}

TEST_CASE("with_emission without overrides, or for a name no material has, draws the file's glow") {
  const std::vector<MaterialDesc> loaded = {named("plaster", kFileGlow)};
  CHECK(same(with_emission(loaded, {}).front().emissive, kFileGlow));
  CHECK(same(with_emission(loaded, {{"window_glass", kWarmLight}}).front().emissive, kFileGlow));
}

TEST_CASE("same_emission tells sets of overrides apart by name and by light") {
  const EmissionOverrides warm = {{"window_glass", kWarmLight}};
  CHECK(same_emission(warm, {{"window_glass", kWarmLight}}));
  CHECK_FALSE(same_emission(warm, {{"window_glass", kFileGlow}}));
  CHECK_FALSE(same_emission(warm, {{"lamp", kWarmLight}}));
  CHECK_FALSE(same_emission(warm, {}));
}
