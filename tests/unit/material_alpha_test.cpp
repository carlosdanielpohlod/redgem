#include <doctest/doctest.h>

#include <vector>

#include "render/material_alpha.hpp"

using namespace redgem;
using namespace redgem::render_math;

namespace {

constexpr float kWind = 0.4f;
constexpr float kMaskCutoff = 0.6f;
constexpr int kSomeImage = 3;  // index of a texture
constexpr AlphaPass kEveryPass[] = {AlphaPass::Shadow, AlphaPass::Opaque, AlphaPass::SeeThrough};

MaterialDesc material(AlphaMode mode, bool textured) {
  MaterialDesc desc;
  desc.alpha_mode = mode;
  desc.alpha_cutoff = kMaskCutoff;
  if (textured) desc.base_color_image = kSomeImage;
  return desc;
}

}  // namespace

TEST_CASE("opaque materials are drawn whole, still and one-sided, and never again see-through") {
  const MaterialDesc wall = material(AlphaMode::Opaque, true);
  const AlphaTest test = alpha_test(wall, kWind, AlphaPass::Opaque);
  CHECK(test.cutoff == kNoCutoff);
  CHECK(test.ceiling == kNoCeiling);
  CHECK(test.sway == kNoWind);
  CHECK_FALSE(test.two_sided);
  CHECK_FALSE(test.mixed);
  CHECK_FALSE(drawn_see_through(wall));
}

TEST_CASE("cut-out materials use their own cutoff in every pass and sway only when textured") {
  for (const AlphaPass pass : kEveryPass) {
    const AlphaTest leaves = alpha_test(material(AlphaMode::Mask, true), kWind, pass);
    CHECK(leaves.cutoff == doctest::Approx(kMaskCutoff));
    CHECK(leaves.sway == doctest::Approx(kWind));
    CHECK(leaves.two_sided);
    const AlphaTest plain = alpha_test(material(AlphaMode::Mask, false), kWind, pass);
    CHECK(plain.cutoff == doctest::Approx(kMaskCutoff));
    CHECK(plain.sway == kNoWind);
    CHECK(plain.two_sided);
  }
  CHECK_FALSE(drawn_see_through(material(AlphaMode::Mask, true)));
}

TEST_CASE("blended materials: their nearly opaque core with the opaque surfaces, the rest mixed afterwards") {
  const MaterialDesc hair = material(AlphaMode::Blend, true);
  CHECK(drawn_see_through(hair));
  const AlphaTest core = alpha_test(hair, kWind, AlphaPass::Opaque);
  CHECK(core.cutoff == doctest::Approx(kBlendCore));
  CHECK(core.ceiling == kNoCeiling);
  CHECK_FALSE(core.mixed);
  const AlphaTest rest = alpha_test(hair, kWind, AlphaPass::SeeThrough);
  CHECK(rest.cutoff == doctest::Approx(kInvisibleAlpha));
  CHECK(rest.ceiling == doctest::Approx(kBlendCore));  // what the core drew is not drawn again
  CHECK(rest.mixed);
  CHECK(rest.two_sided);
  CHECK(rest.sway == doctest::Approx(kWind));  // textured: foliage, unless its object does not sway
}

TEST_CASE("blended textures cast cut-out shadows; untextured ones (glass) whole shadows") {
  CHECK(alpha_test(material(AlphaMode::Blend, true), kWind, AlphaPass::Shadow).cutoff ==
        doctest::Approx(kBlendShadowCutoff));
  const AlphaTest glass = alpha_test(material(AlphaMode::Blend, false), kWind, AlphaPass::Shadow);
  CHECK(glass.cutoff == kNoCutoff);
  CHECK(glass.sway == kNoWind);
  CHECK_FALSE(glass.two_sided);
}

TEST_CASE("a dithered blended material is drawn once, with the opaque surfaces, by a pattern of its alpha") {
  MaterialDesc fur = material(AlphaMode::Blend, true);
  fur.dithered = true;
  CHECK_FALSE(drawn_see_through(fur));
  CHECK(alpha_test(fur, kWind, AlphaPass::Opaque).dithered);
  CHECK_FALSE(alpha_test(fur, kWind, AlphaPass::Shadow).dithered);
  MaterialDesc leaves = material(AlphaMode::Mask, true);
  leaves.dithered = true;  // (only a blended material can be)
  CHECK_FALSE(alpha_test(leaves, kWind, AlphaPass::Opaque).dithered);
}

TEST_CASE("double-sided materials are drawn without backface culling") {
  MaterialDesc sheet = material(AlphaMode::Opaque, false);
  sheet.double_sided = true;
  CHECK(alpha_test(sheet, kWind, AlphaPass::Opaque).two_sided);
}

TEST_CASE("see-through surfaces are drawn the farthest first, and in the order they came when as far") {
  std::vector<SeeThroughDraw> draws = {{4.0f, 0, 0, 1}, {9.0f, 1, 1, 1}, {4.0f, 2, 2, 1}, {1.0f, 3, 3, 1}};
  sort_back_to_front(draws);
  std::vector<int> meshes;
  for (const SeeThroughDraw& draw : draws) meshes.push_back(draw.mesh);
  CHECK(meshes == std::vector<int>{1, 0, 2, 3});
}
