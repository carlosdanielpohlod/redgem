// What the renderer draws into: the shadow map and the reflection targets, made once, and the
// post-processing targets, which follow the size of the output.

#include "render/renderer_impl.hpp"

#include <stdexcept>

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
  depth.id = rlLoadTextureDepth(width, height, false);
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

// A framebuffer with only a depth texture, which the lit shader samples.
RenderTexture2D load_shadow_target(int size) {
  RenderTexture2D target{};
  target.id = rlLoadFramebuffer();
  target.texture.width = size;
  target.texture.height = size;
  rlEnableFramebuffer(target.id);
  target.depth = load_depth_texture(target.id, size, size);
  const bool complete = rlFramebufferComplete(target.id);
  rlDisableFramebuffer();
  if (!complete) throw std::runtime_error("cannot create the shadow map");
  return target;
}

void unload(RenderTexture2D& target) {
  if (loaded(target)) UnloadRenderTexture(target);
  target = RenderTexture2D{};
}

// Whether a target is there and `width` x `height` pixels.
bool fits(const RenderTexture2D& target, int width, int height) {
  return loaded(target) && target.texture.width == width && target.texture.height == height;
}

}  // namespace

void Renderer::Impl::load_fixed_targets() {
  shadow = load_shadow_target(render_math::kShadowMapSize);
  reflection_face = load_target(kReflectionFaceSize, kReflectionFaceSize, kHdrFormat, true);
  panorama = load_target(kPanoramaWidth, kPanoramaHeight, kHdrFormat, false);
  // Around the horizon the panorama wraps; at the poles it must not (up would blend with down).
  rlTextureParameters(panorama.texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
  rlTextureParameters(panorama.texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
}

void Renderer::Impl::ensure_targets(int width, int height) {
  if (fits(hdr, width, height)) return;
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

void Renderer::Impl::release_targets() {
  for (RenderTexture2D* target : {&hdr, &bloom_a, &bloom_b, &ldr, &occlusion_a, &occlusion_b}) unload(*target);
}

void Renderer::Impl::release_fixed_targets() {
  if (loaded(shadow)) {
    rlUnloadTexture(shadow.depth.id);
    rlUnloadFramebuffer(shadow.id);
    shadow = RenderTexture2D{};
  }
  for (RenderTexture2D* target : {&reflection_face, &panorama}) unload(*target);
}

}  // namespace redgem
