#include "render/material_alpha.hpp"

#include <algorithm>

namespace redgem::render_math {
namespace {

bool textured(const MaterialDesc& material) { return material.base_color_image != kNoIndex; }
bool masked(const MaterialDesc& material) { return material.alpha_mode == AlphaMode::Mask; }
bool blended(const MaterialDesc& material) { return material.alpha_mode == AlphaMode::Blend; }

// The opaque surfaces' passes: the lit one, and the one of where they moved.
bool with_the_opaque(AlphaPass pass) { return pass == AlphaPass::Opaque || pass == AlphaPass::Motion; }

// The see-through surfaces' passes: the lit one, and the one of how much they cover.
bool mixed_in(AlphaPass pass) { return pass == AlphaPass::SeeThrough || pass == AlphaPass::Cover; }

// Leaves, grass, fences: a texture's alpha cuts or blends their shape.
bool foliage(const MaterialDesc& material) { return textured(material) && (masked(material) || blended(material)); }

float blended_cutoff(const MaterialDesc& material, AlphaPass pass) {
  if (with_the_opaque(pass)) return kBlendCore;
  if (mixed_in(pass)) return kInvisibleAlpha;
  return textured(material) ? kBlendShadowCutoff : kNoCutoff;
}

float cutoff_of(const MaterialDesc& material, AlphaPass pass) {
  if (masked(material)) return material.alpha_cutoff;
  return blended(material) ? blended_cutoff(material, pass) : kNoCutoff;
}

bool farther(const SeeThroughDraw& a, const SeeThroughDraw& b) {
  if (a.distance != b.distance) return a.distance > b.distance;
  return a.first < b.first;
}

}  // namespace

AlphaTest alpha_test(const MaterialDesc& material, float wind, AlphaPass pass) {
  AlphaTest test;
  test.cutoff = cutoff_of(material, pass);
  test.ceiling = mixed_in(pass) ? kBlendCore : kNoCeiling;
  test.sway = foliage(material) ? wind : kNoWind;
  test.two_sided = material.double_sided || masked(material) || foliage(material);
  test.dithered = with_the_opaque(pass) && blended(material) && material.dithered;
  test.mixed = mixed_in(pass);
  return test;
}

bool drawn_see_through(const MaterialDesc& material) { return blended(material) && !material.dithered; }

void sort_back_to_front(std::vector<SeeThroughDraw>& draws) { std::sort(draws.begin(), draws.end(), farther); }

}  // namespace redgem::render_math
