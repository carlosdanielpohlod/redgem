// Glowing particles kept from washing out to white: when the environment sets a glow limit they
// are gathered apart, then added to the lit picture through a curve that comes near the limit
// however many of them lie over each other (see shaders/particle_glow.hpp).

#include "render/renderer_impl.hpp"

#include "render/shaders/particle_glow.hpp"

namespace redgem {
namespace {

using namespace render_detail;

constexpr ::Color kNothing{0, 0, 0, 0};

}  // namespace

void Renderer::Impl::begin_gathering_glow() {
  ensure_glow_target();
  rlDrawRenderBatchActive();
  rlEnableFramebuffer(glow.id);
  // (Only the color is cleared: depth is not written while particles are drawn, and it is the lit
  // picture's, which they are hidden behind.)
  ClearBackground(kNothing);
}

void Renderer::Impl::add_gathered_glow() {
  BeginShaderMode(glow_limit_shader);
  send(glow_limit_shader, shaders::kGlowLimit, glow_limit);
  rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
  BeginBlendMode(BLEND_CUSTOM_SEPARATE);
  draw_full(glow, hdr.texture.width, hdr.texture.height);
  EndBlendMode();
  EndShaderMode();
}

}  // namespace redgem
