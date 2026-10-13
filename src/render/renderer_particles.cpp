// Drawing particles: squares that face the camera, in one batch per picture they wear.

#include "render/renderer_impl.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::kOff;
using render_math::kOn;
using render_math::kQuadCorners;
using render_math::to_vec3;

constexpr float kParticleGlowBoost = 4.0f;  // how much brighter than its color a glowing particle is

// One particle as a quad of the batch being filled.
void add_particle_quad(const Particle& particle, const render_math::Facing& view) {
  rlCheckRenderBatchLimit(kQuadCorners);
  const auto corners = render_math::particle_corners(particle.position, particle.size_now(), particle.angle, view);
  rlColor4f(particle.color.r, particle.color.g, particle.color.b, particle.alpha_now());
  for (int corner = 0; corner < kQuadCorners; ++corner) {
    rlTexCoord2f(render_math::kQuadU[corner], render_math::kQuadV[corner]);
    rlVertex3f(corners[corner].x, corners[corner].y, corners[corner].z);
  }
}

// How particles mix with what is behind them: ordinary ones cover it, glowing ones add to it.
// The picture's own alpha is left alone: it is not see-through where a particle is.
void blend_particles(bool glowing) {
  rlSetBlendFactorsSeparate(RL_SRC_ALPHA, glowing ? RL_ONE : RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD,
                            RL_FUNC_ADD);
}

// The picture a particle wears, or null for a plain dot.
const Texture2D* worn_by(const Particle& particle, const std::vector<const Texture2D*>& pictures) {
  return particle.image == kNoParticleImage ? nullptr : pictures[static_cast<std::size_t>(particle.image)];
}

// Whether a particle belongs to the batch of the ones that glow (or do not) and wear `picture`.
bool in_batch(const Particle& particle, const ParticleDraw& draw, const Texture2D* picture, bool glowing) {
  return particle.glow == glowing && worn_by(particle, draw.pictures) == picture;
}

}  // namespace

std::vector<const Texture2D*> Renderer::Impl::particle_pictures(const ParticleSystem& system) {
  std::vector<const Texture2D*> pictures;
  for (const std::string& name : system.images()) pictures.push_back(ui_image(name));
  return pictures;
}

void Renderer::Impl::draw_particle_batch(const ParticleDraw& draw, const Texture2D* picture, bool glowing) {
  send(particle_shader, particle_uniforms.textured, picture != nullptr ? kOn : kOff);
  if (picture != nullptr) rlSetTexture(picture->id);
  rlBegin(RL_QUADS);
  for (const Particle& particle : draw.system.particles()) {
    if (in_batch(particle, draw, picture, glowing)) add_particle_quad(particle, draw.view);
  }
  rlEnd();
  rlDrawRenderBatchActive();
  rlSetTexture(0);
}

void Renderer::Impl::draw_particles(const ParticleSystem& system, const Camera3D& camera) {
  if (system.particles().empty()) return;
  const render_math::Facing view =
      render_math::facing(to_vec3(camera.position), to_vec3(camera.target), to_vec3(camera.up));
  const std::vector<const Texture2D*> pictures = particle_pictures(system);
  const std::vector<const Texture2D*> batches = render_math::picture_batches(pictures);
  rlDrawRenderBatchActive();
  rlDisableDepthMask();
  rlDisableBackfaceCulling();
  BeginShaderMode(particle_shader);
  for (const bool glowing : {false, true}) draw_particle_layer({system, pictures, batches, view}, glowing);
  EndShaderMode();
  rlEnableBackfaceCulling();
  rlEnableDepthMask();
}

void Renderer::Impl::draw_particle_layer(const ParticleDraw& draw, bool glowing) {
  send(particle_shader, particle_uniforms.boost, glowing ? kParticleGlowBoost : kOn);
  blend_particles(glowing);
  BeginBlendMode(BLEND_CUSTOM_SEPARATE);
  for (const Texture2D* picture : draw.batches) draw_particle_batch(draw, picture, glowing);
  EndBlendMode();
}

}  // namespace redgem
