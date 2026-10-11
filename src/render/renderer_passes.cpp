// The passes around the scene: the sun's shadow map and the reflections before it, and what
// turns the lit picture into the one on screen after it.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include <algorithm>
#include <utility>

#include "render/shaders.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::kCubeFaces;
using render_math::kShadowExtent;
using render_math::kShadowSunDistance;
using render_math::to_vec3;

// Bloom: what is brighter than the threshold (in linear light) glows.
constexpr float kBloomThreshold = 1.0f;
constexpr int kBloomBlurPasses = 3;
constexpr int kDepthBufferBit = 0x00000100;  // GL_DEPTH_BUFFER_BIT
// The sun's view only needs the depth range that can hold casters and receivers; a tight range
// keeps depth precise (and the shadow bias small).
constexpr double kShadowNearPlane = 1.0;                       // meters
constexpr double kShadowFarPlane = kTwo * kShadowSunDistance;  // meters
constexpr float kRightAngle = 90.0f;                           // degrees: the field of view of one cube face
constexpr float kMinReflectedAngle = 0.03f;                    // radians: only sizeable things are worth reflecting
constexpr int kPanoramaMipsWanted = 1;                         // rlGenTextureMipmaps answers how many levels it made

// One texel of a texture `pixels` wide, in texture coordinates.
float texel_of(int pixels) { return 1.0f / static_cast<float>(pixels); }

// The sun's point of view: straight (no perspective) and wide enough for everything shadowed.
Camera3D sun_camera(const render_math::SunView& fitted) {
  Camera3D sun_view{};
  sun_view.projection = CAMERA_ORTHOGRAPHIC;
  sun_view.fovy = kShadowExtent;
  sun_view.target = fitted.target;
  sun_view.position = fitted.position;
  sun_view.up = fitted.up;
  return sun_view;
}

}  // namespace

void Renderer::Impl::bind_for_the_frame(int sampler_location, int slot, unsigned int texture) {
  rlEnableShader(shader.id);
  rlActiveTextureSlot(slot);
  rlEnableTexture(texture);
  rlSetUniform(sampler_location, &slot, SHADER_UNIFORM_INT, 1);
  rlActiveTextureSlot(0);
}

// --- Sun shadows ------------------------------------------------------------------------------

void Renderer::Impl::draw_shadow_map(const Scene& scene, const Camera3D& camera, const Environment& env) {
  const Vec3 to_sun = normalize(Vec3{-sun_travel.x, -sun_travel.y, -sun_travel.z});
  const float strength = render_math::sun_shadow_strength(to_sun, env.shadows);
  if (strength > 0.0f) draw_sun_depth(scene, camera, to_sun);
  send(shader, shadow_strength_loc, strength);
  send(shader, shadow_texel_loc, texel_of(kShadowMapSize));
  bind_for_the_frame(shadow_map_loc, kShadowTextureSlot, shadow.depth.id);
}

// The scene's depth as the sun sees it, and the matrix that leads the lit shader into that view.
void Renderer::Impl::draw_sun_depth(const Scene& scene, const Camera3D& camera, Vec3 to_sun) {
  const Camera3D sun_view = sun_camera(render_math::fit_sun_view(camera.position, camera.target, to_sun));
  send(depth_shader, depth_time_loc, static_cast<float>(GetTime()));
  const double game_near = rlGetCullDistanceNear();
  const double game_far = rlGetCullDistanceFar();
  rlSetClipPlanes(kShadowNearPlane, kShadowFarPlane);
  const Matrix light_matrix = draw_depth_from(sun_view, scene, to_vec3(camera.position));
  rlSetClipPlanes(game_near, game_far);
  SetShaderValueMatrix(shader, light_matrix_loc, light_matrix);
  send(shader, shaders::kShadowDepthRange, static_cast<float>(kShadowFarPlane - kShadowNearPlane));
}

Matrix Renderer::Impl::draw_depth_from(const Camera3D& sun_view, const Scene& scene, Vec3 viewer) {
  BeginTextureMode(shadow);
  ClearBackground(kWhite);
  BeginMode3D(sun_view);
  const Matrix light_matrix = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  draw_objects(scene, true, viewer);
  EndMode3D();
  EndTextureMode();
  return light_matrix;
}

// --- Reflections ------------------------------------------------------------------------------

void Renderer::Impl::capture_reflection_face(const Scene& scene, const Camera3D& camera) {
  const render_math::CubeFace& face = render_math::kCubeFaceViews[next_face];
  draw_surroundings(scene, face, render_math::reflection_eye(camera.position));
  copy_face_to_panorama(face);
  blur_panorama_levels();
  next_face = render_math::face_after(next_face);
  faces_captured = std::min(faces_captured + 1, kCubeFaces);
  send(shader, reflection_strength_loc, render_math::reflection_strength(faces_captured));
  bind_for_the_frame(reflection_map_loc, kReflectionTextureSlot, panorama.texture.id);
}

// What is around `eye` in the direction of one cube face: the sky and the sizeable objects.
void Renderer::Impl::draw_surroundings(const Scene& scene, const render_math::CubeFace& face, Vector3 eye) {
  Camera3D view{};
  view.projection = CAMERA_PERSPECTIVE;
  view.fovy = kRightAngle;
  view.position = eye;
  view.target = Vector3Add(eye, face.forward);
  view.up = face.up;
  BeginTextureMode(reflection_face);
  ClearBackground(kBlack);
  BeginMode3D(view);
  draw_sky(view);
  draw_objects(scene, false, to_vec3(eye), kMinReflectedAngle);
  EndMode3D();
  EndTextureMode();
}

void Renderer::Impl::copy_face_to_panorama(const render_math::CubeFace& face) {
  const render_math::FaceAxes axes = render_math::face_axes(face);
  const Vector2 size{static_cast<float>(panorama.texture.width), static_cast<float>(panorama.texture.height)};
  rlDisableColorBlend();  // each face replaces its part of the panorama
  BeginTextureMode(panorama);
  BeginShaderMode(panorama_shader);
  send(panorama_shader, shaders::kFaceForward, face.forward);
  send(panorama_shader, shaders::kFaceRight, axes.right);
  send(panorama_shader, shaders::kFaceUp, axes.up);
  send(panorama_shader, shaders::kTargetSize, size);
  draw_full(reflection_face, panorama.texture.width, panorama.texture.height);
  EndShaderMode();
  EndTextureMode();
  rlEnableColorBlend();
}

// Blurrier levels of the panorama, for rougher surfaces.
void Renderer::Impl::blur_panorama_levels() {
  rlEnableTexture(panorama.texture.id);
  int mipmaps = kPanoramaMipsWanted;
  rlGenTextureMipmaps(panorama.texture.id, panorama.texture.width, panorama.texture.height, panorama.texture.format,
                      &mipmaps);
  rlTextureParameters(panorama.texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);
  rlTextureParameters(panorama.texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
  rlDisableTexture();
}

// --- Post-processing --------------------------------------------------------------------------

void Renderer::Impl::draw_full(const RenderTexture2D& source, int width, int height) {
  const Rectangle from{0.0f, 0.0f, static_cast<float>(source.texture.width),
                       -static_cast<float>(source.texture.height)};
  const Rectangle to{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
  DrawTexturePro(source.texture, from, to, Vector2{0.0f, 0.0f}, 0.0f, kWhite);
}

void Renderer::Impl::apply_effect(const RenderTexture2D& target, Shader& effect, const RenderTexture2D& source) {
  BeginTextureMode(target);
  BeginShaderMode(effect);
  draw_full(source, target.texture.width, target.texture.height);
  EndShaderMode();
  EndTextureMode();
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
  const Vector2 occlusion_size{static_cast<float>(occlusion_a.texture.width),
                               static_cast<float>(occlusion_a.texture.height)};
  send(occlusion_shader, shaders::kTargetSize, occlusion_size);
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
  const int width = hdr.texture.width;
  const int height = hdr.texture.height;
  rlBindFramebuffer(RL_READ_FRAMEBUFFER, hdr.id);
  rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, output.id);
  rlBlitFramebuffer(0, 0, width, height, 0, 0, width, height, kDepthBufferBit);
  rlDisableFramebuffer();
}

}  // namespace redgem
