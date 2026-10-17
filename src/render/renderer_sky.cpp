// The sky as a picture (Environment::sky_image): loaded when the environment first names it, then
// drawn behind everything and lighting every surface in place of the gradient sky.

#include "render/renderer_impl.hpp"

#include <cmath>

#include "render/half_float.hpp"
#include "render/sky_light.hpp"
#include "render/sky_picture.hpp"
#include "render/sky_specular.hpp"
#include "render/shaders/uniforms.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::kSkyHarmonics;

constexpr int kLightFormat = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;  // half floats: filtered on phones too
constexpr int kOneLevel = 1;

Texture2D picture_texture(const FloatImage& picture) {
  const std::vector<std::uint16_t> texels = render_math::picture_texels(picture);
  Texture2D texture{};
  texture.id = rlLoadTexture(texels.data(), static_cast<int>(picture.width), static_cast<int>(picture.height),
                             kLightFormat, kOneLevel);
  texture.width = static_cast<int>(picture.width);
  texture.height = static_cast<int>(picture.height);
  texture.mipmaps = kOneLevel;
  texture.format = kLightFormat;
  SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
  // Around the horizon the picture wraps; at the poles it must not.
  rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
  rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
  return texture;
}

unsigned int reflectance_texture() {
  const std::vector<std::uint16_t> table = render_math::reflectance_table();
  const int side = render_math::kReflectanceTableSize;
  const unsigned int id = rlLoadTexture(table.data(), side, side, kLightFormat, kOneLevel);
  rlTextureParameters(id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
  rlTextureParameters(id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);  // read between its entries
  rlTextureParameters(id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
  rlTextureParameters(id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
  return id;
}

// The turn from the world into the picture: the picture is turned by `rotation`, so a direction
// of the world is turned back by as much to find where it falls on it.
Vector2 turn_into_picture(float rotation) { return {std::cos(-rotation), std::sin(-rotation)}; }

}  // namespace

void Renderer::Impl::use_sky_picture(const Environment& env) {
  if (env.sky_image == sky.name) return;
  release_sky_picture();
  sky.name = env.sky_image;
  if (sky.name.empty()) return;
  const std::optional<FloatImage> picture = load_sky_picture(images_dir, sky.name, render_math::kSkyPictureWidest);
  if (picture) upload_sky_picture(*picture);
}

void Renderer::Impl::upload_sky_picture(const FloatImage& picture) {
  const render_math::SkyLight light = render_math::sky_light_of(picture);
  sky.picture = picture_texture(picture);
  sky.reflections = rlLoadTextureCubemap(light.reflections.data(), render_math::kSkyCubeSize, kLightFormat,
                                         render_math::kSkyCubeLevels);
  sky.irradiance = light.irradiance;
  if (sky.reflectance == 0) sky.reflectance = reflectance_texture();
  sky_material.maps[MATERIAL_MAP_ALBEDO].texture = sky.picture;
}

void Renderer::Impl::release_sky_picture() {
  if (loaded(sky.picture)) UnloadTexture(sky.picture);
  if (sky.reflections != 0) rlUnloadTexture(sky.reflections);
  sky.picture = Texture2D{};
  sky.reflections = 0;
  sky.name.clear();
  sky_material.maps[MATERIAL_MAP_ALBEDO].texture = sky_default_texture;
}

void Renderer::Impl::send_sky_light(const Environment& env) {
  const float pictured = loaded(sky.picture) ? render_math::kOn : render_math::kOff;
  const Vector2 turn = turn_into_picture(env.sky_rotation);
  send(sky_shader, shaders::kSkyLight, pictured);
  for (const Shader* program : {&shader, &sky_shader}) {
    send(*program, shaders::kSkyTurn, turn);
    send(*program, shaders::kSkyIntensity, env.sky_intensity);
  }
  if (!loaded(sky.picture)) return;
  const render_math::HarmonicFloats harmonics = render_math::harmonic_floats(sky.irradiance);
  SetShaderValueV(shader, lit_uniforms.sky_irradiance, harmonics.data(), SHADER_UNIFORM_VEC3, kSkyHarmonics);
  bind_cube_for_the_frame(shader, lit_uniforms.sky_reflections, kSkyReflectionSlot, sky.reflections);
  bind_for_the_frame(shader, lit_uniforms.sky_reflectance, kSkyReflectanceSlot, sky.reflectance);
}

void Renderer::Impl::release_sky_light() {
  release_sky_picture();
  if (sky.reflectance != 0) rlUnloadTexture(sky.reflectance);
  sky.reflectance = 0;
}

}  // namespace redgem
