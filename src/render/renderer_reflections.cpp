// Reflections: the camera's surroundings, captured one cube face per frame into a panorama the
// lit shader reflects.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include <algorithm>

#include "render/shaders/uniforms.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::kCubeFaces;
using render_math::to_vec3;

constexpr float kRightAngle = 90.0f;         // degrees: the field of view of one cube face
constexpr float kMinReflectedAngle = 0.03f;  // radians: only sizeable things are worth reflecting
constexpr int kPanoramaMipsWanted = 1;       // rlGenTextureMipmaps answers how many levels it made

// What `eye` sees in the direction of one cube face.
Camera3D face_view(const render_math::CubeFace& face, Vector3 eye) {
  Camera3D view{};
  view.projection = CAMERA_PERSPECTIVE;
  view.fovy = kRightAngle;
  view.position = eye;
  view.target = Vector3Add(eye, face.forward);
  view.up = face.up;
  return view;
}

}  // namespace

void Renderer::Impl::capture_reflection_face(const Camera3D& camera) {
  const render_math::CubeFace& face = render_math::kCubeFaceViews[next_face];
  draw_surroundings(face, render_math::reflection_eye(camera.position));
  copy_face_to_panorama(face);
  blur_panorama_levels();
  next_face = render_math::face_after(next_face);
  faces_captured = std::min(faces_captured + 1, kCubeFaces);
  send(shader, lit_uniforms.reflection_strength, render_math::reflection_strength(faces_captured));
  bind_for_the_frame(shader, lit_uniforms.reflection_map, kReflectionTextureSlot, panorama.texture.id);
}

// What is around `eye` in the direction of one cube face: the sky and the sizeable objects, their
// see-through parts too.
void Renderer::Impl::draw_surroundings(const render_math::CubeFace& face, Vector3 eye) {
  const Camera3D view = face_view(face, eye);
  BeginTextureMode(reflection_face);
  ClearBackground(kBlack);
  BeginMode3D(view);
  draw_sky(view);
  const std::size_t first = draw_objects(false, to_vec3(eye), kMinReflectedAngle);
  draw_see_through(first, to_vec3(eye));
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

}  // namespace redgem
