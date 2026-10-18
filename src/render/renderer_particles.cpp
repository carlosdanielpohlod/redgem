// Drawing particles: squares that face the camera, laid out once a frame (see particle_layout) and
// drawn run by run, each run one picture mixed in one way.

#include "render/renderer_impl.hpp"

#include <raymath.h>

namespace redgem {
namespace {

using namespace render_detail;
using render_math::kOff;
using render_math::kOn;
using render_math::to_vec3;

constexpr float kParticleGlowBoost = 4.0f;  // how much brighter than its color a glowing particle is

// How particles mix with what is behind them: ordinary ones cover it, glowing ones add to it.
// The picture's own alpha is left alone: it is not see-through where a particle is.
void blend_particles(bool glowing) {
  rlSetBlendFactorsSeparate(RL_SRC_ALPHA, glowing ? RL_ONE : RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD,
                            RL_FUNC_ADD);
  rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
}

void bind_texture(int slot, unsigned int texture) {
  rlActiveTextureSlot(slot);
  rlEnableTexture(texture);
  rlActiveTextureSlot(0);
}

}  // namespace

void Renderer::Impl::batch_particle_pictures(const ParticleSystem& system) {
  particle_pictures.clear();
  for (const std::string& name : system.images()) particle_pictures.push_back(ui_image(name));
  render_math::picture_batches(particle_pictures, particle_batches, particle_batch_of);
}

void Renderer::Impl::draw_particles(const ParticleSystem& system, const Camera3D& camera) {
  if (system.particles().empty()) return;
  prepare_particles(system, camera);
  begin_particles();
  draw_particle_runs(false);
  glow_gathered = gathers_glow();
  if (glow_gathered) begin_gathering_glow();
  draw_particle_runs(true);
  end_particles();
}

void Renderer::Impl::prepare_particles(const ParticleSystem& system, const Camera3D& camera) {
  const render_math::Facing view =
      render_math::facing(to_vec3(camera.position), to_vec3(camera.target), to_vec3(camera.up));
  batch_particle_pictures(system);
  render_math::lay_out_particles(system.particles(), particle_batch_of, particle_batches.size(), view, particle_layout);
  rlDrawRenderBatchActive();
  if (particle_layout.any_soft || gathers_glow()) share_scene_depth();
  send_particles(particle_buffer, particle_layout.vertices);
}

void Renderer::Impl::draw_particle_runs(bool glowing) {
  for (const render_math::ParticleRun& run : particle_layout.runs) {
    if (run.glow == glowing) draw_particle_run(run);
  }
}

void Renderer::Impl::begin_particles() {
  rlDisableDepthMask();
  rlDisableBackfaceCulling();
  rlEnableShader(particle_shader.id);
  const Matrix view_projection = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  rlSetUniformMatrix(particle_shader.locs[SHADER_LOC_MATRIX_MVP], view_projection);
  if (!particle_layout.any_soft) return;
  bind_for_the_frame(particle_shader, particle_uniforms.scene_depth, kSceneDepthSlot, scene_depth.depth.id);
  send(particle_shader, particle_uniforms.near_plane, static_cast<float>(rlGetCullDistanceNear()));
  send(particle_shader, particle_uniforms.far_plane, static_cast<float>(rlGetCullDistanceFar()));
}

void Renderer::Impl::draw_particle_run(const render_math::ParticleRun& run) {
  blend_particles(run.glow);
  draw_particle_picture(run);
}

void Renderer::Impl::draw_particle_picture(const render_math::ParticleRun& run) {
  const Texture2D* picture = particle_batches[run.batch];
  send(particle_shader, particle_uniforms.boost, run.glow ? kParticleGlowBoost : kOn);
  send(particle_shader, particle_uniforms.textured, picture != nullptr ? kOn : kOff);
  bind_texture(0, picture != nullptr ? picture->id : rlGetTextureIdDefault());
  render_detail::draw_particle_run(particle_buffer, run);
}

void Renderer::Impl::end_particles() {
  if (glow_gathered) rlEnableFramebuffer(drawing_framebuffer());
  rlSetBlendMode(BLEND_ALPHA);
  bind_texture(0, 0);
  rlDisableShader();
  rlEnableBackfaceCulling();
  rlEnableDepthMask();
}

unsigned int Renderer::Impl::drawing_framebuffer() const {
  return scene_samples.framebuffer != 0 ? scene_samples.framebuffer : hdr.id;
}

void Renderer::Impl::share_scene_depth() {
  const int width = hdr.texture.width;
  const int height = hdr.texture.height;
  if (drawing_framebuffer() != hdr.id) copy_buffer(drawing_framebuffer(), hdr.id, width, height, Buffer::Depth);
  if (particle_layout.any_soft) copy_soft_particle_depth();
  rlEnableFramebuffer(drawing_framebuffer());  // drawing goes on where it was
}

void Renderer::Impl::copy_soft_particle_depth() {
  ensure_scene_depth_target();
  copy_buffer(hdr.id, scene_depth.id, hdr.texture.width, hdr.texture.height, Buffer::Depth);
}

}  // namespace redgem
