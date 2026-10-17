#include <doctest/doctest.h>

#include <set>
#include <string>

#include "physics/body_config.hpp"
#include "render/environment.hpp"
#include "scene/props.hpp"
#include "scene/scene.hpp"

using namespace redgem;

namespace {

constexpr float kThickFog = 0.5f;

}  // namespace

TEST_CASE("the properties scenes carry keep the names files already use") {
  CHECK(std::string(props::kScript) == "script");
  CHECK(std::string(props::kBody) == "body");
  CHECK(std::string(props::kCollider) == "collider");
  CHECK(std::string(props::kTrigger) == "trigger");
  CHECK(std::string(props::kTag) == "tag");
  CHECK(std::string(props::kPrefab) == "prefab");
  CHECK(std::string(props::kEnvironment) == "environment");
  CHECK(std::string(props::environment::kFogDensity) == "fog_density");
}

TEST_CASE("no two properties share a name") {
  const char* const names[] = {props::kScript,
                               props::kBody,
                               props::kCollider,
                               props::kTrigger,
                               props::kTag,
                               props::kMass,
                               props::kVisible,
                               props::kWheel,
                               props::kTerrain,
                               props::kTerrainSize,
                               props::kTerrainCells,
                               props::kPrefab,
                               props::kTint,
                               props::kVisibleFrom,
                               props::kVisibleTo,
                               props::kCastShadow,
                               props::kEnvironment,
                               props::environment::kSkyTop,
                               props::environment::kFogDensity};
  std::set<std::string> seen;
  for (const char* name : names) CHECK_MESSAGE(seen.insert(name).second, name);
}

TEST_CASE("what is written under the contract's names is what the engine's parts read") {
  // The physics reads the body and the collider, the renderer the environment: one spelling for all.
  const BodyConfig body = body_config_from_props(
      {{props::kBody, std::string(props::kBodyDynamic)}, {props::kCollider, std::string(props::kColliderSphere)}});
  CHECK(body.type == BodyType::Dynamic);
  CHECK(body.collider == ColliderShape::Sphere);
  const Environment foggy = environment_from_props({{props::environment::kFogDensity, static_cast<double>(kThickFog)}});
  CHECK(foggy.fog_density == doctest::Approx(kThickFog));
}
