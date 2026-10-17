// Edges smoothed with the frames before (temporal anti-aliasing, see temporal.hpp): the lit pass's
// view moved by less than a pixel each frame, where its opaque surfaces moved from, where its
// particles cover it, and the frame mixed with the ones before.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include "render/shaders/uniforms.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::to_vec3;

constexpr int kTexelFormat = RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;  // as the frame's texels
constexpr int kOneLevel = 1;
constexpr ::Color kNoMotion{0, 0, 0, 0};  // nothing drawn, nothing covered

Matrix inverse_then(const Matrix& undone, const Matrix& then) { return MatrixMultiply(MatrixInvert(undone), then); }

void send_matrix(const Shader& program, int location, const Matrix& matrix) {
  SetShaderValueMatrix(program, location, matrix);
}

// Covered: a particle's alpha added to what is covered already, on the alpha channel alone.
void blend_cover() {
  rlColorMask(false, false, false, true);
  rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
  rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
}

}  // namespace

void Renderer::Impl::begin_temporal(const Environment& env, const Camera3D& camera) {
  temporal.on = env.temporal_antialias;
  send(shader, lit_uniforms.noise_shift, temporal.on ? render_math::noise_shift(temporal.frame) : 0.0f);
  if (!temporal.on) {
    temporal.seen_before = false;  // turned on again, it starts over
    return;
  }
  ensure_temporal_targets();
  const render_math::TemporalView view = temporal_view(camera);
  temporal.history_valid = temporal.seen_before && render_math::keeps_history(temporal.view, view);
  temporal.view = view;
  temporal.seen_before = true;
  temporal.time = static_cast<float>(GetTime());
}

render_math::TemporalView Renderer::Impl::temporal_view(const Camera3D& camera) const {
  const Vec3 position = to_vec3(camera.position);
  return {hdr.texture.width, hdr.texture.height, position, normalize(to_vec3(camera.target) - position),
          camera.fovy,       scene_uploads};
}

void Renderer::Impl::jitter_view() {
  const Matrix projection = rlGetMatrixProjection();
  temporal.view_projection = MatrixMultiply(rlGetMatrixModelview(), projection);
  const Vector2 offset = render_math::jitter_of(temporal.frame);
  rlSetMatrixProjection(render_math::jittered(projection, offset, hdr.texture.width, hdr.texture.height));
}

// --- Where things moved from --------------------------------------------------------------------

void Renderer::Impl::draw_motion(std::size_t first, Vec3 viewer) {
  rlEnableFramebuffer(motion.id);
  ClearBackground(kNoMotion);
  rlDisableColorBlend();  // motion is written as it is
  ObjectPass objects{begin_mesh_pass(motion_shader, motion_instance_slot), motion_uniforms.skinned,
                     render_math::AlphaPass::Motion, render_math::View{}};
  send_motion_frame(viewer);
  for (const render_math::InstanceBatch& batch : batches.batches()) draw_batch(batch, first, objects);
  end_mesh_pass();
  rlEnableColorBlend();
  rlEnableFramebuffer(drawing_framebuffer());
}

void Renderer::Impl::send_motion_frame(Vec3 viewer) {
  send(motion_shader, motion_uniforms.view_position, render_math::to_raylib(viewer));
  send(motion_shader, motion_uniforms.time, temporal.time);
  send(motion_shader, motion_uniforms.previous_time, temporal.previous_time);
  send(motion_shader, motion_uniforms.noise_shift, render_math::noise_shift(temporal.frame));
  send_matrix(motion_shader, motion_uniforms.view_projection, temporal.view_projection);
  send_matrix(motion_shader, motion_uniforms.previous_view_projection, temporal.previous_view_projection);
  bind_for_the_frame(motion_shader, motion_uniforms.frame_texels, kFrameTexelSlot, texel_texture);
  bind_for_the_frame(motion_shader, motion_uniforms.previous_texels, kPreviousTexelSlot, previous_texels.texture);
}

// Particles write no motion: where they cover the picture the frames before count for less.
void Renderer::Impl::mark_particle_cover() {
  rlDrawRenderBatchActive();
  rlEnableFramebuffer(motion.id);  // (its depth hides the particles behind surfaces)
  begin_particles();
  blend_cover();
  for (const render_math::ParticleRun& run : particle_layout.runs) draw_particle_picture(run);
  rlColorMask(true, true, true, true);
  end_particles();
  rlEnableFramebuffer(drawing_framebuffer());
}

// --- The last frame's records and bones ---------------------------------------------------------

// The frame's texels are copied before they change (or, when their texture was made again
// bigger, after: that frame's motion is then only the camera's).
void Renderer::Impl::keep_previous_texels() {
  if (previous_texels.rows != texel_texture_rows) remake_previous_texels();
  copy_color(previous_texels.from, previous_texels.to, render_math::kTexelsPerRow, texel_texture_rows);
}

void Renderer::Impl::remake_previous_texels() {
  release_previous_texels();
  previous_texels.texture =
      rlLoadTexture(nullptr, render_math::kTexelsPerRow, texel_texture_rows, kTexelFormat, kOneLevel);
  previous_texels.rows = texel_texture_rows;
  previous_texels.from = rlLoadFramebuffer();
  rlFramebufferAttach(previous_texels.from, texel_texture, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
  previous_texels.to = rlLoadFramebuffer();
  rlFramebufferAttach(previous_texels.to, previous_texels.texture, RL_ATTACHMENT_COLOR_CHANNEL0,
                      RL_ATTACHMENT_TEXTURE2D, 0);
  rlDisableFramebuffer();
}

// (A framebuffer is let go with its depth alone: their colors are the textures'.)
void Renderer::Impl::release_previous_texels() {
  for (const unsigned int framebuffer : {previous_texels.from, previous_texels.to}) {
    if (framebuffer != 0) rlUnloadFramebuffer(framebuffer);
  }
  if (previous_texels.texture != 0) rlUnloadTexture(previous_texels.texture);
  previous_texels = PreviousTexels{};
}

// --- Mixing the frames ------------------------------------------------------------------------

void Renderer::Impl::resolve_temporal() {
  const std::size_t written = 1 - temporal.latest;
  const RenderTexture2D& before = histories[temporal.latest];
  BeginTextureMode(histories[written]);
  BeginShaderMode(temporal_shader);
  send_temporal_inputs(before);
  draw_full(hdr, hdr.texture.width, hdr.texture.height);
  EndShaderMode();
  EndTextureMode();
  temporal.latest = written;
}

void Renderer::Impl::send_temporal_inputs(const RenderTexture2D& before) {
  SetShaderValueTexture(temporal_shader, temporal_uniforms.history, before.texture);
  SetShaderValueTexture(temporal_shader, temporal_uniforms.motion, motion.texture);
  SetShaderValueTexture(temporal_shader, temporal_uniforms.motion_depth, motion.depth);
  send_matrix(temporal_shader, temporal_uniforms.reprojection,
              inverse_then(temporal.view_projection, temporal.previous_view_projection));
  const Vector2 texel{1.0f / static_cast<float>(hdr.texture.width), 1.0f / static_cast<float>(hdr.texture.height)};
  send(temporal_shader, temporal_uniforms.texel_size, texel);
  send(temporal_shader, temporal_uniforms.history_valid, motion_needed() ? render_math::kOn : render_math::kOff);
}

void Renderer::Impl::end_temporal() {
  if (!temporal.on) return;
  temporal.previous_view_projection = temporal.view_projection;
  temporal.previous_time = temporal.time;
  ++temporal.frame;
}

const RenderTexture2D& Renderer::Impl::lit_picture() const { return temporal.on ? histories[temporal.latest] : hdr; }

}  // namespace redgem
