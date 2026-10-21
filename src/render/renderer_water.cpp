// Water draws after opaque surfaces, sampling a separate copy of their linear light and depth.
#include "render/renderer_impl.hpp"

#include <algorithm>

namespace redgem {
namespace {
using namespace render_detail;
// These slots belong to the motion and particle programs in other passes; lit has them free.
constexpr int kWaterDepthSlot = 14;
constexpr int kWaterBackgroundSlot = 15;

bool motion_pass(render_math::AlphaPass pass) {
  return pass == render_math::AlphaPass::Motion || pass == render_math::AlphaPass::Cover;
}

template <typename T>
T for_pass(render_math::AlphaPass pass, const T& lit, const T& motion, const T& depth) {
  if (pass == render_math::AlphaPass::Shadow) return depth;
  return motion_pass(pass) ? motion : lit;
}
}  // namespace

void Renderer::Impl::capture_water_background() {
  rlDrawRenderBatchActive();
  const int width = hdr.texture.width;
  const int height = hdr.texture.height;
  copy_buffer(drawing_framebuffer(), water_background.id, width, height, Buffer::Color);
  copy_buffer(drawing_framebuffer(), water_background.id, width, height, Buffer::Depth);
  rlEnableFramebuffer(drawing_framebuffer());
  bind_for_the_frame(shader, lit_uniforms.water_depth, kWaterDepthSlot, water_background.depth.id);
  bind_for_the_frame(shader, lit_uniforms.water_background, kWaterBackgroundSlot, water_background.texture.id);
  send(shader, lit_uniforms.water_ready, render_math::kOn);
  send(shader, lit_uniforms.water_planes,
       Vector2{static_cast<float>(rlGetCullDistanceNear()), static_cast<float>(rlGetCullDistanceFar())});
}

void Renderer::Impl::send_water(const WaterDesc& water, render_math::AlphaPass pass) {
  const Shader& program = for_pass(pass, shader, motion_shader, depth_shader);
  const WaterUniforms& uniforms = for_pass(pass, lit_water, motion_water, depth_water);
  send(program, uniforms.enabled, water.enabled ? render_math::kOn : render_math::kOff);
  send(program, uniforms.waves, Vector3{water.wave_height, water.wavelength, water.speed});
  if (program.id != shader.id) return;
  send(shader, lit_uniforms.water_absorption, render_math::to_raylib(water.absorption));
  send(shader, lit_uniforms.water_surface, Vector2{water.foam_width, water.refraction});
}

float Renderer::Impl::mesh_wave_height(int mesh) const {
  if (!lit_features.water) return 0.0f;
  float height = 0.0f;
  for (const GpuPrimitive& part : meshes[static_cast<std::size_t>(mesh)]) {
    const WaterDesc& water = factors_of(part.material).water;
    if (water.enabled) height = std::max(height, water.wave_height);
  }
  return height;
}
}  // namespace redgem
