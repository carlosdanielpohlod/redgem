// Edges smoothed with the frames before (temporal anti-aliasing, see temporal.hpp): the lit pass's
// view moved by less than a pixel each frame, where its opaque surfaces moved from, where its
// particles cover it, and the frame mixed with the ones before.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include "core/log.hpp"
#include "render/instance_records.hpp"
#include "render/shaders/uniforms.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::to_vec3;

using render_math::kTexelFormat;
using render_math::kTexelLevels;
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

// Whether a frame is smoothed with the ones before: the environment asks, the view allows it (not
// the editor's) and the graphics card can.
bool temporal_wanted(const Environment& env, bool allowed, bool unsupported) {
  return env.temporal_antialias && allowed && !unsupported;
}

}  // namespace

void Renderer::Impl::begin_temporal(const Environment& env, const Camera3D& camera) {
  temporal.on = temporal_wanted(env, temporal_allowed, temporal_unsupported);
  send(shader, lit_uniforms.noise_shift, temporal.on ? render_math::noise_shift(temporal.frame) : 0.0f);
  if (!temporal.on) {
    temporal.seen_before = false;  // turned on again, it starts over
    release_temporal_targets();
    release_previous_texels();
    return;
  }
  ensure_temporal_targets();
  const render_math::TemporalView view = temporal_view(camera);
  temporal.history_valid = temporal.seen_before && render_math::keeps_history(temporal.view, view);
  temporal.view = view;
  temporal.seen_before = true;
  temporal.time = now();
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

void Renderer::Impl::mark_see_through_cover(Vec3 viewer) {
  rlEnableFramebuffer(motion.id);
  ObjectPass pass{begin_mesh_pass(motion_shader, motion_instance_slot), motion_uniforms.skinned,
                  render_math::AlphaPass::Cover, render_math::View{}};
  blend_cover();
  rlDisableDepthMask();
  send_motion_frame(viewer);
  for (const render_math::SeeThroughDraw& draw : see_through) draw_see_through_copies(draw, pass);
  end_mesh_pass();
  rlEnableDepthMask();
  rlColorMask(true, true, true, true);
  rlSetBlendMode(BLEND_ALPHA);
  rlEnableFramebuffer(drawing_framebuffer());
}

// --- The last frame's records and bones ---------------------------------------------------------

// The frame's texels are copied before they change (or, when their texture was made again
// bigger, after: that frame's motion is then only the camera's).
void Renderer::Impl::keep_previous_texels() {
  if (previous_texels.rows != texel_texture_rows) remake_previous_texels();
  if (temporal_unsupported) return;
  copy_buffer(previous_texels.from, previous_texels.to, render_math::kTexelsPerRow, texel_texture_rows, Buffer::Color);
  rlDisableFramebuffer();
}

void Renderer::Impl::remake_previous_texels() {
  release_previous_texels();
  previous_texels.texture =
      rlLoadTexture(nullptr, render_math::kTexelsPerRow, texel_texture_rows, kTexelFormat, kTexelLevels);
  previous_texels.rows = texel_texture_rows;
  previous_texels.from = rlLoadFramebuffer();
  rlFramebufferAttach(previous_texels.from, texel_texture, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
  previous_texels.to = rlLoadFramebuffer();
  rlFramebufferAttach(previous_texels.to, previous_texels.texture, RL_ATTACHMENT_COLOR_CHANNEL0,
                      RL_ATTACHMENT_TEXTURE2D, 0);
  const bool copyable = rlFramebufferComplete(previous_texels.from) && rlFramebufferComplete(previous_texels.to);
  rlDisableFramebuffer();
  if (!copyable) give_up_temporal();
}

// A card that cannot draw into float textures (OpenGL ES without EXT_color_buffer_float) cannot
// copy the frame's texels: frames are drawn without the ones before from then on.
void Renderer::Impl::give_up_temporal() {
  log::warning("the graphics card cannot copy float textures: drawing without temporal anti-aliasing");
  temporal_unsupported = true;
  temporal.on = false;
  temporal.history_valid = false;
  release_previous_texels();
}

void Renderer::Impl::note_fresh_record(const Scene& scene, const GameObject& object, float first_bone,
                                       std::size_t bones) {
  const auto slot = static_cast<std::size_t>(object.id);
  const ObjectHandle owner = scene.handle(object.id);
  const bool fresh = record_owners[slot] != owner || record_sources[slot].first_bone != first_bone;
  record_owners[slot] = owner;
  if (!fresh) return;
  fresh_texels.emplace_back(render_math::record_texel(scattered_records + slot), render_math::kTexelsPerInstance);
  if (bones > 0) fresh_texels.emplace_back(static_cast<std::size_t>(first_bone), bones * render_math::kTexelsPerMatrix);
}

// Fresh records and bones go into the last frame's copy as they are now.
void Renderer::Impl::carry_fresh_texels() {
  if (previous_texels.texture == 0) fresh_texels.clear();  // (no copy to carry them into)
  for (const auto& [first, count] : fresh_texels) {
    for (const render_math::TexelSpan& span : render_math::texel_spans(first, count)) {
      const float* values = texels.row(span.first.y) + span.first.x * render_math::kFloatsPerTexel;
      rlUpdateTexture(previous_texels.texture, span.first.x, span.first.y, span.count, 1, kTexelFormat, values);
    }
  }
  fresh_texels.clear();
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
  const Vector2 texel{texel_of(hdr.texture.width), texel_of(hdr.texture.height)};
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
