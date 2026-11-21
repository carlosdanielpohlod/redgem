// The view model: a first-person weapon or hands (objects with `view_model = true`, and what is
// under them). It is drawn after the world, over a cleared depth, so it never goes into a wall
// the player presses against. It has a field of view of its own (Camera.view_model_fov), is lit
// by the sun without the world's shadows and by a fill light from the eye (readable indoors), and
// casts no shadow on the world (the world's passes leave it out: see `seen`). No frustum culls
// it: it is always in front of the camera.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include <algorithm>

#include "render/temporal.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::to_vec3;

// The fill light: a white point light at the eye, reaching a little past arm's length.
constexpr float kFillIntensity = 0.08f;
constexpr float kFillRange = 3.0f;
constexpr float kNoShadow = 0.0f;
constexpr int kNoLocalShadows = 0;

bool in_view_model(const ObjectSight& sight) { return sight.drawn && sight.view_model; }

PointLight fill_light(Vec3 eye) {
  PointLight fill;
  fill.position = eye;
  fill.color = Vec3::one() * kFillIntensity;
  fill.range = kFillRange;
  return fill;
}

}  // namespace

void Renderer::Impl::draw_view_model(const Camera3D& camera) {
  if (std::none_of(sights.begin(), sights.end(), in_view_model)) return;
  rlDrawRenderBatchActive();
  clear_depth();
  const Matrix world_projection = rlGetMatrixProjection();
  rlSetMatrixProjection(view_model_projection());
  const Vec3 viewer = to_vec3(camera.position);
  light_view_model(viewer);
  const std::size_t first = draw_view_model_objects(viewer);
  if (motion_needed()) mark_view_model_cover(first, viewer);
  rlSetMatrixProjection(world_projection);
}

Matrix Renderer::Impl::view_model_projection() const {
  const double near_plane = rlGetCullDistanceNear();
  const double far_plane = rlGetCullDistanceFar();
  const Matrix projection = MatrixPerspective(view_model_fov * DEG2RAD, view_aspect, near_plane, far_plane);
  if (!temporal.on) return projection;
  return render_math::jittered(projection, render_math::jitter_of(temporal.frame), hdr.texture.width,
                               hdr.texture.height);
}

// The sun as the world has it, but no shadow (the world's would darken it indoors), and the fill.
void Renderer::Impl::light_view_model(Vec3 viewer) {
  send(shader, lit_uniforms.shadows.strength, kNoShadow);
  send(shader, lit_uniforms.local_shadow_count, kNoLocalShadows);
  const render_math::PointLightUniforms fill = render_math::pack_point_lights({fill_light(viewer)});
  send_lights(frame_sun.direction, frame_sun.color, frame_sun.ambient, fill);
}

std::size_t Renderer::Impl::draw_view_model_objects(Vec3 viewer) {
  batches.start();
  gather_view_model(false);
  gather_view_model(true);
  const std::size_t first = append_slots(slots, batches.slots());
  ObjectPass objects = object_pass(false, render_math::View{});
  for (const render_math::InstanceBatch& batch : batches.batches()) draw_batch(batch, first, objects);
  face_forward(objects);
  end_mesh_pass();
  draw_see_through(first, viewer);
  return first;
}

void Renderer::Impl::gather_view_model(bool mirrored) {
  for (std::size_t slot = 0; slot < sights.size(); ++slot) {
    const ObjectSight& sight = sights[slot];
    if (in_view_model(sight) && sight.mirrored == mirrored) batches.add(sight.mesh, scattered_records + slot, mirrored);
  }
}

// (Over the world's depth, which it is in front of whatever that says.)
void Renderer::Impl::mark_view_model_cover(std::size_t first, Vec3 viewer) {
  rlDrawRenderBatchActive();
  rlEnableFramebuffer(motion.id);
  ObjectPass pass{begin_mesh_pass(motion_shader, motion_instance_slot), motion_uniforms.skinned,
                  motion_morph_locations(), render_math::AlphaPass::Cover, render_math::View{}};
  blend_cover();
  rlDisableDepthTest();
  send_motion_frame(viewer);
  for (const render_math::InstanceBatch& batch : batches.batches()) draw_batch(batch, first, pass);
  face_forward(pass);
  end_mesh_pass();
  rlEnableDepthTest();
  rlColorMask(true, true, true, true);
  rlSetBlendMode(BLEND_ALPHA);
  rlEnableFramebuffer(drawing_framebuffer());
}

}  // namespace redgem
