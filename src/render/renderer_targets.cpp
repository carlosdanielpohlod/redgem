// What the renderer draws into: the shadow map and the reflection targets, made once, and the
// post-processing targets, which follow the size of the output.

#include "render/renderer_impl.hpp"

#include <stdexcept>
#include <string>

#include "core/log.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::downscaled;

// Bloom is blurred at a fraction of the resolution, which is both cheaper and wider.
constexpr int kBloomDownscale = 4;
constexpr int kOcclusionDownscale = 2;  // ambient occlusion is computed at half resolution
constexpr int kDepthFormat24 = 19;      // raylib's internal id of a 24-bit depth texture
constexpr int kOneMipLevel = 1;         // a texture with no smaller copies of itself
// Reflections: one cube face at a time, gathered into a panorama.
constexpr int kReflectionFaceSize = 256;  // pixels along each side
constexpr int kPanoramaWidth = 1024;      // pixels
constexpr int kPanoramaHeight = 512;
constexpr int kHdrFormat = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;  // linear light, brighter than white allowed
constexpr int kScreenFormat = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;   // what ends up on screen

// A depth texture (not a render buffer: the ambient occlusion pass and the lit shader read
// depth), attached to the framebuffer being set up.
Texture2D load_depth_texture(unsigned int framebuffer, int width, int height) {
  Texture2D depth{};
  depth.id = render_detail::depth_texture(width, height);  // (the format a picture of several samples has)
  depth.width = width;
  depth.height = height;
  depth.format = kDepthFormat24;
  depth.mipmaps = kOneMipLevel;
  rlFramebufferAttach(framebuffer, depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
  return depth;
}

// An empty color texture, attached to the framebuffer being set up.
Texture2D load_color_texture(unsigned int framebuffer, int width, int height, int format) {
  Texture2D color{};
  color.id = rlLoadTexture(nullptr, width, height, format, kOneMipLevel);
  color.width = width;
  color.height = height;
  color.format = format;
  color.mipmaps = kOneMipLevel;
  rlFramebufferAttach(framebuffer, color.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
  return color;
}

RenderTexture2D load_target(int width, int height, int format, bool with_depth) {
  RenderTexture2D target{};
  target.id = rlLoadFramebuffer();
  rlEnableFramebuffer(target.id);
  target.texture = load_color_texture(target.id, width, height, format);
  if (with_depth) target.depth = load_depth_texture(target.id, width, height);
  const bool complete = rlFramebufferComplete(target.id);
  rlDisableFramebuffer();
  if (!complete) {
    UnloadRenderTexture(target);
    throw std::runtime_error("cannot create a render target (no float framebuffer support?)");
  }
  SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
  SetTextureWrap(target.texture, TEXTURE_WRAP_CLAMP);
  return target;
}

// A framebuffer with only a depth texture, which shaders sample (the sun's shadow map, what soft
// particles fade into).
RenderTexture2D load_depth_target(int width, int height) {
  RenderTexture2D target{};
  target.id = rlLoadFramebuffer();
  target.texture.width = width;
  target.texture.height = height;
  rlEnableFramebuffer(target.id);
  target.depth = load_depth_texture(target.id, width, height);
  const bool complete = rlFramebufferComplete(target.id);
  rlDisableFramebuffer();
  if (!complete) throw std::runtime_error("cannot create a depth target");
  return target;
}

void unload_depth_target(RenderTexture2D& target) {
  if (!loaded(target)) return;
  rlUnloadTexture(target.depth.id);
  rlUnloadFramebuffer(target.id);
  target = RenderTexture2D{};
}

// A target that draws on another's depth (which stays that other's when it is unloaded).
RenderTexture2D load_target_on(const Texture2D& depth, int format) {
  RenderTexture2D target{};
  target.id = rlLoadFramebuffer();
  rlEnableFramebuffer(target.id);
  target.texture = load_color_texture(target.id, depth.width, depth.height, format);
  rlFramebufferAttach(target.id, depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
  const bool complete = rlFramebufferComplete(target.id);
  rlDisableFramebuffer();
  if (!complete) throw std::runtime_error("cannot create a render target on another's depth");
  return target;
}

// (raylib deletes whatever depth a framebuffer has when it unloads it: the other's is let go first.)
void unload_target_on(RenderTexture2D& target) {
  if (!loaded(target)) return;
  rlFramebufferAttach(target.id, 0, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
  rlDisableFramebuffer();
  UnloadRenderTexture(target);
  target = RenderTexture2D{};
}

void unload(RenderTexture2D& target) {
  if (loaded(target)) UnloadRenderTexture(target);
  target = RenderTexture2D{};
}

// What a new target holds is undefined: it is cleared before anything reads it.
void clear(const RenderTexture2D& target) {
  BeginTextureMode(target);
  ClearBackground(::Color{0, 0, 0, 0});
  EndTextureMode();
}

// Whether a target is there and `width` x `height` pixels.
bool fits(const RenderTexture2D& target, int width, int height) {
  return loaded(target) && target.texture.width == width && target.texture.height == height;
}

// Whether the picture of several samples was already asked for at this size (made or not).
bool asked_before(const Multisampled& target, int width, int height, int samples) {
  return target.samples == samples && target.width == width && target.height == height;
}

// Whether several samples a pixel were asked for and the graphics card could not make them.
bool refused(const Multisampled& target) { return target.samples > 0 && target.framebuffer == 0; }

}  // namespace

void Renderer::Impl::load_fixed_targets() {
  shadow = load_depth_target(render_math::kShadowMapSize, render_math::kShadowMapSize);
  reflection_face = load_target(kReflectionFaceSize, kReflectionFaceSize, kHdrFormat, true);
  panorama = load_target(kPanoramaWidth, kPanoramaHeight, kHdrFormat, false);
  // Around the horizon the panorama wraps; at the poles it must not (up would blend with down).
  rlTextureParameters(panorama.texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
  rlTextureParameters(panorama.texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
}

void Renderer::Impl::ensure_targets(int width, int height, int asked) {
  if (!fits(hdr, width, height)) make_targets(width, height);
  const int samples = samples_unusable ? 0 : asked;
  if (asked_before(scene_samples, width, height, samples)) return;
  render_detail::release(scene_samples);
  scene_samples = samples > 0 ? load_multisampled(width, height, samples) : Multisampled{0, 0, 0, width, height, 0};
  if (refused(scene_samples)) {
    log::warning("the graphics card cannot draw " + std::to_string(samples) + " samples a pixel: drawing one");
  }
}

void Renderer::Impl::resolve_samples() {
  if (resolve(scene_samples, hdr.id)) return;
  log::warning("the graphics card cannot copy the picture of several samples a pixel: drawing one");
  samples_unusable = true;
  render_detail::release(scene_samples);
}

void Renderer::Impl::make_targets(int width, int height) {
  release_targets();
  const int bloom_width = downscaled(width, kBloomDownscale);
  const int bloom_height = downscaled(height, kBloomDownscale);
  hdr = load_target(width, height, kHdrFormat, true);
  bloom_a = load_target(bloom_width, bloom_height, kHdrFormat, false);
  bloom_b = load_target(bloom_width, bloom_height, kHdrFormat, false);
  ldr = load_target(width, height, kScreenFormat, false);
  const int occlusion_width = downscaled(width, kOcclusionDownscale);
  const int occlusion_height = downscaled(height, kOcclusionDownscale);
  occlusion_a = load_target(occlusion_width, occlusion_height, kScreenFormat, false);
  occlusion_b = load_target(occlusion_width, occlusion_height, kScreenFormat, false);
}

void Renderer::Impl::ensure_glow_target() {
  if (!loaded(glow)) glow = load_target_on(hdr.depth, kHdrFormat);
}

void Renderer::Impl::ensure_scene_depth_target() {
  if (!loaded(scene_depth)) scene_depth = load_depth_target(hdr.texture.width, hdr.texture.height);
}

void Renderer::Impl::ensure_temporal_targets() {
  const int width = hdr.texture.width;
  const int height = hdr.texture.height;
  if (fits(motion, width, height)) return;
  motion = load_target(width, height, kHdrFormat, true);
  SetTextureWrap(motion.depth, TEXTURE_WRAP_CLAMP);  // read around each pixel, at the edges too
  for (RenderTexture2D& history : histories) history = load_target(width, height, kHdrFormat, false);
  for (const RenderTexture2D* target : {&motion, &histories[0], &histories[1]}) clear(*target);
}

void Renderer::Impl::release_temporal_targets() {
  for (RenderTexture2D* target : {&motion, &histories[0], &histories[1]}) unload(*target);
}

void Renderer::Impl::release_targets() {
  unload_target_on(glow);  // before the lit picture, whose depth it draws on
  for (RenderTexture2D* target : {&hdr, &bloom_a, &bloom_b, &ldr, &occlusion_a, &occlusion_b}) unload(*target);
  release_temporal_targets();
  render_detail::release(scene_samples);
  unload_depth_target(scene_depth);
}

void Renderer::Impl::release_fixed_targets() {
  unload_depth_target(shadow);
  for (RenderTexture2D* target : {&reflection_face, &panorama}) unload(*target);
}

}  // namespace redgem
