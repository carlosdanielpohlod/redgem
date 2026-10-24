// Quality presets: what each allows, and what a level's environment gets under them.
#include <doctest/doctest.h>

#include "render/environment.hpp"
#include "render/scatter_chunks.hpp"

using namespace redgem;

namespace {

// An environment that asks for everything there is.
Environment asking_for_everything() {
  Environment env = default_environment();
  env.msaa = config::kMostSamples;
  env.temporal_antialias = true;
  env.shadow_cascades = config::kMostCascades;
  env.shadow_distance = 200.0f;
  env.ambient_occlusion = 0.8f;
  env.bloom = 0.6f;
  return env;
}

}  // namespace

TEST_CASE("ultra gives a level everything it asks for, exactly") {
  const Environment asked = asking_for_everything();
  const Environment got = within_quality(asked, preset_caps(QualityPreset::Ultra), GpuAbilities{});
  CHECK(got.msaa == asked.msaa);
  CHECK(got.temporal_antialias);
  CHECK(got.shadow_cascades == asked.shadow_cascades);
  CHECK(got.shadow_distance == asked.shadow_distance);
  CHECK(got.ambient_occlusion == asked.ambient_occlusion);
  CHECK(got.bloom == asked.bloom);
  CHECK(preset_caps(QualityPreset::Custom) == preset_caps(QualityPreset::Ultra));
}

TEST_CASE("each preset below allows less than the one above it") {
  const QualityPreset order[] = {QualityPreset::Low, QualityPreset::Medium, QualityPreset::High, QualityPreset::Ultra};
  for (int at = 1; at < 4; ++at) {
    const QualityCaps lower = preset_caps(order[at - 1]);
    const QualityCaps higher = preset_caps(order[at]);
    CHECK(lower.msaa < higher.msaa);
    CHECK(lower.shadow_cascades < higher.shadow_cascades);
    CHECK(lower.shadow_distance < higher.shadow_distance);
    CHECK(lower.scatter_density < higher.scatter_density);
    CHECK(lower.scatter_distance <= higher.scatter_distance);
    CHECK(lower.sky_detail <= higher.sky_detail);
  }
  const QualityCaps low = preset_caps(QualityPreset::Low);
  CHECK_FALSE(low.temporal_antialias);
  CHECK(low.ambient_occlusion == 0.0f);
  CHECK(low.bloom == 0.0f);
}

TEST_CASE("a preset holds a level under it and never raises what it asked for") {
  const Environment medium =
      within_quality(asking_for_everything(), preset_caps(QualityPreset::Medium), GpuAbilities{});
  CHECK(medium.msaa == 2);
  CHECK(medium.shadow_cascades == 2);
  CHECK(medium.shadow_distance == doctest::Approx(100.0f));
  CHECK(medium.ambient_occlusion == doctest::Approx(0.4f));
  Environment plain = default_environment();
  plain.msaa = 0;
  plain.shadow_cascades = 1;
  plain.temporal_antialias = false;
  const Environment high = within_quality(plain, preset_caps(QualityPreset::High), GpuAbilities{});
  CHECK(high.msaa == 0);
  CHECK(high.shadow_cascades == 1);
  CHECK_FALSE(high.temporal_antialias);
}

TEST_CASE("shadows stay long enough for a cascade, unless the level had none to shorten") {
  QualityCaps caps;
  caps.shadow_distance = 0.0f;
  Environment env = asking_for_everything();
  CHECK(within_quality(env, caps, GpuAbilities{}).shadow_distance == doctest::Approx(1.0f));
  env.shadow_distance = 0.0f;
  CHECK(within_quality(env, caps, GpuAbilities{}).shadow_distance == 0.0f);
}

TEST_CASE("what the graphics card cannot do is dropped whatever the quality") {
  GpuAbilities weak;
  weak.multisampling = false;
  weak.temporal = false;
  const Environment got = within_quality(asking_for_everything(), preset_caps(QualityPreset::Ultra), weak);
  CHECK(got.msaa == 0);
  CHECK_FALSE(got.temporal_antialias);
}

TEST_CASE("a custom quality's samples are a power of two, at most what the level asked for") {
  QualityCaps caps;
  caps.msaa = 6;
  CHECK(within_quality(asking_for_everything(), caps, GpuAbilities{}).msaa == 4);
  caps.msaa = 1;
  CHECK(within_quality(asking_for_everything(), caps, GpuAbilities{}).msaa == 0);
}

TEST_CASE("a quality draws a share of a scattered set's copies, spread over them, and nearer") {
  ScatterSet set;
  set.visible_to = 100.0f;
  set.lods = {ScatterLod{1, 40.0f}};
  for (int at = 0; at < 100; ++at) {
    set.places.push_back(redgem::Transform{});
    set.tints.push_back(Vec3{static_cast<float>(at), 0.0f, 0.0f});
  }
  const ScatterSet half = render_math::thinned_set(set, 0.5f, 0.5f);
  CHECK(half.places.size() == 50);
  CHECK(half.tints.size() == 50);
  CHECK(half.tints[1].x - half.tints[0].x == doctest::Approx(2.0f));  // every other one
  CHECK(half.visible_to == doctest::Approx(50.0f));
  CHECK(half.lods[0].from == doctest::Approx(20.0f));
  const ScatterSet quarter = render_math::thinned_set(set, 0.25f, 1.0f);
  CHECK(quarter.places.size() == 25);
  CHECK(quarter.visible_to == doctest::Approx(100.0f));
  const ScatterSet whole = render_math::thinned_set(set, 1.0f, 1.0f);
  CHECK(whole.places.size() == 100);
  CHECK(render_math::thinned_set(set, 0.0f, 1.0f).places.empty());
}
