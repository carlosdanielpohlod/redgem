// From the lit picture to the one on screen: ambient occlusion, bloom, tone mapping and
// anti-aliasing, each a full-screen pass.

#include "render/renderer_impl.hpp"

#include <utility>

#include "render/shaders/uniforms.hpp"

namespace redgem {
namespace {

using namespace render_detail;

// Bloom: what is brighter than the threshold (in linear light) glows.
constexpr float kBloomThreshold = 1.0f;
constexpr int kBloomBlurPasses = 3;
constexpr int kDepthBufferBit = 0x00000100;  // GL_DEPTH_BUFFER_BIT

// One texel of a texture `pixels` wide, in texture coordinates.
float texel_of(int pixels) { return 1.0f / static_cast<float>(pixels); }

Vector2 size_of(const RenderTexture2D& target) {
  return {static_cast<float>(target.texture.width), static_cast<float>(target.texture.height)};
}

// Draws `source` through a full-screen shader into `target`.
void apply_effect(const RenderTexture2D& target, const Shader& effect, const RenderTexture2D& source) {
  BeginTextureMode(target);
  BeginShaderMode(effect);
  draw_full(source, target.texture.width, target.texture.height);
  EndShaderMode();
  EndTextureMode();
}

}  // namespace

void render_detail::copy_depth(unsigned int from, unsigned int to, int width, int height) {
  rlBindFramebuffer(RL_READ_FRAMEBUFFER, from);
  rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, to);
  rlBlitFramebuffer(0, 0, width, height, 0, 0, width, height, kDepthBufferBit);
}

void render_detail::draw_full(const RenderTexture2D& source, int width, int height) {
  const Rectangle from{0.0f, 0.0f, static_cast<float>(source.texture.width),
                       -static_cast<float>(source.texture.height)};
  const Rectangle to{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
  DrawTexturePro(source.texture, from, to, Vector2{0.0f, 0.0f}, 0.0f, kWhite);
}

void Renderer::Impl::post_process(const Environment& env, const RenderTexture2D* output) {
  rlDrawRenderBatchActive();
  rlDisableColorBlend();  // these passes replace pixels (alpha carries data, not transparency)
  if (env.ambient_occlusion > 0.0f) draw_occlusion();
  draw_bloom();
  tone_map(env);
  antialias(output);
  rlEnableColorBlend();
}

// Ambient occlusion from the depth the scene left behind, then a blur to hide its sampling noise.
void Renderer::Impl::draw_occlusion() {
  RenderTexture2D depth_view{};
  depth_view.texture = hdr.depth;
  send(occlusion_shader, shaders::kTargetSize, size_of(occlusion_a));
  send(occlusion_shader, shaders::kTanHalfFov, view_tan_half_fov);
  send(occlusion_shader, shaders::kAspect, view_aspect);
  send(occlusion_shader, shaders::kNearPlane, static_cast<float>(rlGetCullDistanceNear()));
  send(occlusion_shader, shaders::kFarPlane, static_cast<float>(rlGetCullDistanceFar()));
  apply_effect(occlusion_a, occlusion_shader, depth_view);
  blur(occlusion_a, occlusion_b);
}

// The bright parts of the picture, at a fraction of its size, blurred until they glow.
void Renderer::Impl::draw_bloom() {
  send(bright_shader, shaders::kThreshold, kBloomThreshold);
  apply_effect(bloom_a, bright_shader, hdr);
  for (int i = 0; i < kBloomBlurPasses; ++i) blur(bloom_a, bloom_b);
}

void Renderer::Impl::blur(const RenderTexture2D& picture, const RenderTexture2D& scratch) {
  const int step_loc = GetShaderLocation(blur_shader, shaders::kBlurStep);
  send(blur_shader, step_loc, Vector2{texel_of(picture.texture.width), 0.0f});
  apply_effect(scratch, blur_shader, picture);
  send(blur_shader, step_loc, Vector2{0.0f, texel_of(picture.texture.height)});
  apply_effect(picture, blur_shader, scratch);
}

// Linear HDR + bloom + ambient occlusion -> the graded picture, with its luma in alpha.
void Renderer::Impl::tone_map(const Environment& env) {
  BeginTextureMode(ldr);
  BeginShaderMode(composite_shader);
  send_grade(env);
  SetShaderValueTexture(composite_shader, GetShaderLocation(composite_shader, shaders::kBloomTexture), bloom_a.texture);
  send(composite_shader, shaders::kOcclusionAmount, env.ambient_occlusion);
  SetShaderValueTexture(composite_shader, GetShaderLocation(composite_shader, shaders::kOcclusionTexture),
                        occlusion_a.texture);
  draw_full(hdr, hdr.texture.width, hdr.texture.height);
  EndShaderMode();
  EndTextureMode();
}

void Renderer::Impl::send_grade(const Environment& env) {
  const std::pair<const char*, float> grade[] = {{shaders::kBloomStrength, env.bloom},
                                                 {shaders::kExposure, env.exposure},
                                                 {shaders::kVignette, env.vignette},
                                                 {shaders::kContrast, env.contrast},
                                                 {shaders::kSaturation, env.saturation},
                                                 {shaders::kGrain, env.grain},
                                                 {shaders::kAberration, env.aberration},
                                                 {shaders::kMotionBlur, motion_blur},
                                                 {shaders::kGradeTime, static_cast<float>(GetTime())}};
  for (const auto& [name, value] : grade) send(composite_shader, name, value);
}

// Smooths the edges of the tone mapped picture into `output` (the screen when null).
void Renderer::Impl::antialias(const RenderTexture2D* output) {
  const int width = hdr.texture.width;
  const int height = hdr.texture.height;
  if (output != nullptr) BeginTextureMode(*output);
  BeginShaderMode(antialias_shader);
  send(antialias_shader, shaders::kTexelSize, Vector2{texel_of(width), texel_of(height)});
  draw_full(ldr, width, height);
  EndShaderMode();
  if (output == nullptr) {
    rlDrawRenderBatchActive();
    return;
  }
  EndTextureMode();
  copy_depth_to(*output);
}

// The scene's depth goes along, so overlays drawn into `output` afterwards are occluded by it.
void Renderer::Impl::copy_depth_to(const RenderTexture2D& output) {
  copy_depth(hdr.id, output.id, hdr.texture.width, hdr.texture.height);
  rlDisableFramebuffer();
}

}  // namespace redgem
