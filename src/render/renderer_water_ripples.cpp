#include "render/renderer_impl.hpp"

namespace redgem {
namespace {
using namespace render_detail;
using render_math::AlphaPass;

void send_packet(const Shader& program, const WaterRippleUniforms& uniforms, const WaterRipplePacket& packet) {
  send(program, uniforms.count, packet.count);
  if (packet.count == 0) return;
  SetShaderValueV(program, uniforms.waves, packet.waves.data(), SHADER_UNIFORM_VEC4, packet.count);
  SetShaderValueV(program, uniforms.radii, packet.radii.data(), SHADER_UNIFORM_FLOAT, packet.count);
}

bool ripple_motion(AlphaPass pass, bool active) { return pass == AlphaPass::Cover && active; }

void begin_ripple_motion() {
  rlColorMask(true, true, true, true);
  rlDisableColorBlend();
  rlEnableDepthMask();
}

void end_ripple_motion() {
  rlEnableColorBlend();
  rlDisableDepthMask();
  rlColorMask(false, false, false, true);
  rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
  rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
}
}  // namespace

void Renderer::Impl::reset_water_ripples() {
  water_ripples = {};
  previous_water_ripples = {};
  water_handles.clear();
}

bool Renderer::Impl::active_water_ripples() const {
  return water_ripples.count > 0 || previous_water_ripples.count > 0;
}

void Renderer::Impl::clear_water_ripple_uniforms() {
  if (!water_ripple_uniforms_dirty) return;
  send(shader, lit_water.ripples.count, 0);
  send(shader, lit_water.previous_ripples.count, 0);
  send(motion_shader, motion_water.ripples.count, 0);
  send(motion_shader, motion_water.previous_ripples.count, 0);
  water_ripple_uniforms_dirty = false;
}

void Renderer::Impl::register_water_handles(const Scene& scene, const WaterRippleSnapshot& snapshot) {
  for (std::size_t index = 0; index < snapshot.count; ++index) {
    const auto handle = snapshot.ripples[index].surface;
    const ObjectId id = scene.resolve(handle);
    if (id != kNoObject) water_handles[id] = handle;
  }
}

void Renderer::Impl::prepare_water_ripples(const Scene& scene, const WaterRippleSnapshot* snapshot) {
  water_ripples = validated_water_ripples(snapshot);
  water_handles.clear();
  if (!active_water_ripples()) {
    clear_water_ripple_uniforms();
    return;
  }
  register_water_handles(scene, water_ripples);
  register_water_handles(scene, previous_water_ripples);
}

ObjectHandle Renderer::Impl::water_owner(std::size_t record) const {
  if (record < scattered_records) return kNoHandle;
  const auto found = water_handles.find(static_cast<ObjectId>(record - scattered_records));
  return found == water_handles.end() ? kNoHandle : found->second;
}

bool Renderer::Impl::send_water_ripples(const GpuPrimitive& primitive, std::size_t record, AlphaPass pass) {
  const auto owner = water_owner(record);
  water_ripple_uniforms_dirty = true;
  const auto current = water_ripple_packet(water_ripples, owner, primitive.primitive);
  const auto before = previous_water_ripple_packet(previous_water_ripples, water_ripples, owner, primitive.primitive);
  const bool motion = pass == AlphaPass::Cover;
  const auto& program = motion ? motion_shader : shader;
  const auto& uniforms = motion ? motion_water : lit_water;
  send_packet(program, uniforms.ripples, current);
  send_packet(program, uniforms.previous_ripples, before);
  return current.count > 0 || before.count > 0;
}

void Renderer::Impl::draw_water_copy(const GpuPrimitive& primitive, const InstanceRun& run, std::size_t record,
                                     ObjectPass& pass) {
  use_material(primitive.material, pass);
  const bool moving = ripple_motion(pass.alpha, send_water_ripples(primitive, record, pass.alpha));
  if (moving) {
    begin_ripple_motion();
    send(motion_shader, motion_uniforms.covering, render_math::kOff);
  }
  draw_primitive(primitive, run, pass);
  if (moving) {
    send(motion_shader, motion_uniforms.covering, render_math::kOn);
    end_ripple_motion();
  }
}

void Renderer::Impl::draw_water_copies(const GpuPrimitive& primitive, const InstanceRun& run, ObjectPass& pass) {
  if (!active_water_ripples()) {
    draw_primitive(primitive, run, pass);
    return;
  }
  for (std::size_t copy = 0; copy < run.count; ++copy) {
    const auto offset = run.first + copy;
    const auto record = static_cast<std::size_t>(batches.slots()[offset - see_through_slots_first]);
    draw_water_copy(primitive, {run.buffer, offset, 1}, record, pass);
  }
}

}  // namespace redgem
