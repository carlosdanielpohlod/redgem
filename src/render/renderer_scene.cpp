// Drawing the scene itself: its objects, the sky around them and the particles among them.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include <algorithm>
#include <utility>

#include "render/culling.hpp"
#include "render/shaders.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::kOff;
using render_math::kOn;
using render_math::kQuadCorners;
using render_math::model_matrix;
using render_math::to_raylib;
using render_math::to_vec3;

constexpr float kParticleGlowBoost = 4.0f;   // how much brighter than its color a glowing particle is
constexpr float kLightFadeDistance = 12.0f;  // meters over which the farthest used lights fade out

// One particle as a quad of the batch being filled.
void add_particle_quad(const Particle& particle, const render_math::Facing& view) {
  rlCheckRenderBatchLimit(kQuadCorners);
  const auto corners = render_math::particle_corners(particle.position, particle.size_now(), particle.angle, view);
  rlColor4f(particle.color.r, particle.color.g, particle.color.b, particle.alpha_now());
  for (int corner = 0; corner < kQuadCorners; ++corner) {
    rlTexCoord2f(render_math::kQuadU[corner], render_math::kQuadV[corner]);
    rlVertex3f(corners[corner].x, corners[corner].y, corners[corner].z);
  }
}

// How particles mix with what is behind them: ordinary ones cover it, glowing ones add to it.
// The picture's own alpha is left alone: it is not see-through where a particle is.
void blend_particles(bool glowing) {
  rlSetBlendFactorsSeparate(RL_SRC_ALPHA, glowing ? RL_ONE : RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD,
                            RL_FUNC_ADD);
}

}  // namespace

// --- Camera, lights, places ---------------------------------------------------------------------

Camera3D Renderer::Impl::camera_for(const Scene& scene) const {
  Camera3D camera{};
  camera.projection = CAMERA_PERSPECTIVE;
  const ObjectId id = scene.main_camera();
  if (id == kNoObject) {
    camera.position = {0.0f, config::kDefaultCameraDistance * kHalf, config::kDefaultCameraDistance};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = render_math::kUpAxis;
    camera.fovy = config::kDefaultFovYDegrees;
    return camera;
  }
  const Transform world = scene.world_transform(id);
  camera.position = to_raylib(world.position);
  camera.target = to_raylib(world.position + world.forward());
  camera.up = to_raylib(world.up());
  const float fov = scene.at(id).camera->fov_y_radians;
  camera.fovy = fov > 0.0f ? radians_to_degrees(fov) : config::kDefaultFovYDegrees;
  return camera;
}

void Renderer::Impl::apply_lights(const Scene& scene, const Environment& env, Vec3 viewer) {
  SceneLights lights = scene_lights(scene);
  const std::vector<PointLight> nearest =
      nearest_lights(std::move(lights.points), viewer, shaders::kMaxPointLights, kLightFadeDistance);
  const Vector3 sun_direction = to_raylib(lights.sun_direction);
  const Vector3 sun_color = to_raylib(lights.sun_color);
  send_lights(sun_direction, sun_color, env.ambient, render_math::pack_point_lights(nearest));
  send_atmosphere(env);
  send_sky_dome(env, sun_direction, sun_color);
  wind = env.wind;
  sun_travel = sun_direction;
}

void Renderer::Impl::send_lights(Vector3 sun_direction, Vector3 sun_color, float ambient,
                                 const render_math::PointLightUniforms& points) {
  send(shader, sun_direction_loc, sun_direction);
  send(shader, sun_color_loc, sun_color);
  send(shader, ambient_loc, ambient);
  send(shader, point_count_loc, points.count);
  if (points.count == 0) return;
  SetShaderValueV(shader, point_positions_loc, points.positions, SHADER_UNIFORM_VEC3, points.count);
  SetShaderValueV(shader, point_colors_loc, points.colors, SHADER_UNIFORM_VEC3, points.count);
  SetShaderValueV(shader, point_ranges_loc, points.ranges, SHADER_UNIFORM_FLOAT, points.count);
}

// What surfaces are lit by from all around, what hides them far away, and the time foliage sways by.
void Renderer::Impl::send_atmosphere(const Environment& env) {
  send(shader, sky_top_loc, to_raylib(env.sky_top));
  send(shader, sky_horizon_loc, to_raylib(env.sky_horizon));
  send(shader, ground_loc, to_raylib(env.ground));
  send(shader, fog_color_loc, to_raylib(env.fog_color));
  send(shader, fog_density_loc, env.fog_density);
  send(shader, time_loc, static_cast<float>(GetTime()));
}

// The sky dome shows the same sky the surfaces are lit by.
void Renderer::Impl::send_sky_dome(const Environment& env, Vector3 sun_direction, Vector3 sun_color) {
  send(sky_shader, shaders::kSkyTop, to_raylib(env.sky_top));
  send(sky_shader, shaders::kSkyHorizon, to_raylib(env.sky_horizon));
  send(sky_shader, shaders::kGroundColor, to_raylib(env.ground));
  send(sky_shader, shaders::kSunDirection, sun_direction);
  send(sky_shader, shaders::kSunColor, sun_color);
  send(sky_shader, shaders::kSunGlow, env.sun_glow);
}

void Renderer::Impl::place_objects(const Scene& scene) { render_math::world_transforms(scene, worlds); }

bool Renderer::Impl::pose_skeleton(const Scene& scene, const GameObject& object) {
  if (object.skin < 0 || static_cast<std::size_t>(object.skin) >= skins.size()) return false;
  const SkinDesc& skin = skins[static_cast<std::size_t>(object.skin)];
  const std::size_t count =
      std::min({object.joints.size(), skin.inverse_bind.size(), static_cast<std::size_t>(config::kMaxSkinJoints)});
  bone_matrices.resize(count);
  for (std::size_t j = 0; j < count; ++j) {
    if (!scene.is_alive(object.joints[j])) return false;
    bone_matrices[j] = MatrixMultiply(render_math::from_column_major(skin.inverse_bind[j]),
                                      model_matrix(worlds[static_cast<std::size_t>(object.joints[j])]));
  }
  return count > 0;
}

// --- Materials --------------------------------------------------------------------------------

const MaterialDesc& Renderer::Impl::factors_of(int material) const {
  const bool known = material != kNoIndex && static_cast<std::size_t>(material) < material_factors.size();
  return known ? material_factors[static_cast<std::size_t>(material)] : fallback_factors;
}

bool Renderer::Impl::apply_material(int index, bool depth_pass) {
  const MaterialDesc& factors = factors_of(index);
  const render_math::AlphaTest test = render_math::alpha_test(factors, wind);
  const Shader& pass_shader = depth_pass ? depth_shader : shader;
  if (!depth_pass) send_surface(factors);
  send(pass_shader, depth_pass ? depth_alpha_cutoff_loc : alpha_cutoff_loc, test.cutoff);
  send(pass_shader, depth_pass ? depth_wind_loc : wind_loc, test.sway);
  return test.two_sided;
}

// How a material's surface answers light.
void Renderer::Impl::send_surface(const MaterialDesc& factors) {
  const bool has_occlusion_map = factors.occlusion_image != kNoIndex;
  send(shader, has_normal_map_loc, factors.normal_image != kNoIndex ? kOn : kOff);
  send(shader, normal_scale_loc, factors.normal_scale);
  send(shader, has_surface_map_loc, factors.metallic_roughness_image != kNoIndex ? kOn : kOff);
  send(shader, occlusion_strength_loc, has_occlusion_map ? factors.occlusion_strength : kOff);
  send(shader, metallic_loc, factors.metallic);
  send(shader, roughness_loc, factors.roughness);
  send(shader, emissive_loc, to_raylib(factors.emissive));
  send(shader, has_emissive_map_loc, factors.emissive_image != kNoIndex ? kOn : kOff);
}

// --- Drawing meshes -----------------------------------------------------------------------
// The engine's own draw call. raylib's DrawMesh sets the whole pipeline up again for every
// mesh (every matrix, every texture slot, bound and unbound); a scene is thousands of meshes
// per pass, so here what belongs to the pass is set once, what belongs to a material only
// when the material changes, and a mesh costs its matrices and one draw.

void Renderer::Impl::begin_pass(const Shader& pass_shader) {
  rlDrawRenderBatchActive();  // whatever raylib still had queued goes first
  pass_view_projection = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  rlEnableShader(pass_shader.id);
  if (pass_shader.locs[SHADER_LOC_MATRIX_VIEW] != kNoLocation) {
    rlSetUniformMatrix(pass_shader.locs[SHADER_LOC_MATRIX_VIEW], rlGetMatrixModelview());
  }
  if (pass_shader.locs[SHADER_LOC_MATRIX_PROJECTION] != kNoLocation) {
    rlSetUniformMatrix(pass_shader.locs[SHADER_LOC_MATRIX_PROJECTION], rlGetMatrixProjection());
  }
  // Each kind of texture has its own slot, the same for every material.
  for (const int map : kMaterialMaps) {
    const int location = pass_shader.locs[SHADER_LOC_MAP_DIFFUSE + map];
    if (location != kNoLocation) rlSetUniform(location, &map, SHADER_UNIFORM_INT, 1);
  }
}

void Renderer::Impl::bind_material(const Material& material, const Shader& pass_shader) {
  for (const int map : kMaterialMaps) {
    if (pass_shader.locs[SHADER_LOC_MAP_DIFFUSE + map] == kNoLocation) continue;
    rlActiveTextureSlot(map);
    const unsigned int texture = material.maps[map].texture.id;
    if (texture != 0) {
      rlEnableTexture(texture);
    } else {
      rlDisableTexture();
    }
  }
  const int color_location = pass_shader.locs[SHADER_LOC_COLOR_DIFFUSE];
  if (color_location == kNoLocation) return;
  const auto color = render_math::color_floats(material.maps[MATERIAL_MAP_DIFFUSE].color);
  rlSetUniform(color_location, color.data(), SHADER_UNIFORM_VEC4, 1);
}

void Renderer::Impl::draw_mesh(const Mesh& mesh, const Matrix& model, const Shader& pass_shader,
                               const std::vector<Matrix>* bones) {
  if (pass_shader.locs[SHADER_LOC_MATRIX_MODEL] != kNoLocation) {
    rlSetUniformMatrix(pass_shader.locs[SHADER_LOC_MATRIX_MODEL], model);
  }
  if (pass_shader.locs[SHADER_LOC_MATRIX_NORMAL] != kNoLocation) {
    rlSetUniformMatrix(pass_shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(model)));
  }
  rlSetUniformMatrix(pass_shader.locs[SHADER_LOC_MATRIX_MVP], MatrixMultiply(model, pass_view_projection));
  if (bones != nullptr && pass_shader.locs[SHADER_LOC_BONE_MATRICES] != kNoLocation) {
    rlSetUniformMatrices(pass_shader.locs[SHADER_LOC_BONE_MATRICES], bones->data(), static_cast<int>(bones->size()));
  }
  rlEnableVertexArray(mesh.vaoId);
  if (mesh.indices != nullptr) {
    rlDrawVertexArrayElements(0, mesh.triangleCount * config::kVerticesPerTriangle, nullptr);
  } else {
    rlDrawVertexArray(0, mesh.vertexCount);
  }
}

void Renderer::Impl::end_pass() {
  rlDisableVertexArray();
  for (const int map : kMaterialMaps) {
    rlActiveTextureSlot(map);
    rlDisableTexture();
  }
  rlActiveTextureSlot(0);
  rlDisableShader();
}

// --- Objects ----------------------------------------------------------------------------------

void Renderer::Impl::draw_objects(const Scene& scene, bool depth_pass, Vec3 viewer, float detail_angle) {
  const Matrix view_projection = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  const render_math::View view{frustum_from_matrix(MatrixToFloatV(view_projection).v), viewer,
                               render_math::smallest_angle_drawn(detail_angle, depth_pass)};
  ObjectPass objects{depth_pass ? depth_shader : shader, depth_pass ? depth_skinned_loc : skinned_loc, depth_pass,
                     view};
  const std::size_t count = std::min(scene.size(), worlds.size());
  begin_pass(objects.shader);
  for (std::size_t slot = 0; slot < count; ++slot) {
    const GameObject& object = scene.at(static_cast<ObjectId>(slot));
    if (!render_math::has_something_to_draw(object, show_prefabs, meshes.size())) continue;
    draw_object(scene, object, worlds[slot], objects);
  }
  end_pass();
}

void Renderer::Impl::draw_object(const Scene& scene, const GameObject& object, const Transform& world,
                                 ObjectPass& pass) {
  const bool skinned = pose_skeleton(scene, object);
  if (!in_view(object, world, skinned, pass.view)) return;
  const Matrix transform = model_matrix(world);
  if (!pass.depth) {
    const Vector3 tint = to_raylib(object.tint);
    rlSetUniform(tint_loc, &tint, SHADER_UNIFORM_VEC3, 1);
  }
  for (const GpuPrimitive& primitive : meshes[static_cast<std::size_t>(object.mesh)]) {
    draw_primitive(primitive, transform, skinned, pass);
  }
}

// False for what the view cannot see, what looks too small in it, and levels of detail meant for
// another distance. (A mesh without bounds is judged by where its object is.)
bool Renderer::Impl::in_view(const GameObject& object, const Transform& world, bool skinned,
                             const render_math::View& view) const {
  const Bounds& bounds = mesh_bounds[static_cast<std::size_t>(object.mesh)];
  if (!bounds.valid) {
    return !outside_distance_range(world.position, view.viewer, object.visible_from, object.visible_to);
  }
  const render_math::BoundingSphere sphere =
      skinned ? render_math::skinned_sphere(bounds, bone_matrices.front()) : render_math::world_sphere(bounds, world);
  return render_math::worth_drawing(view, sphere, object.visible_from, object.visible_to);
}

// Consecutive parts often share a material: its textures and uniforms are then already set.
void Renderer::Impl::use_material(int material, ObjectPass& pass) {
  if (material == pass.last_material) return;
  pass.last_two_sided = apply_material(material, pass.depth);
  bind_material(material == kNoIndex ? fallback_material : materials[static_cast<std::size_t>(material)], pass.shader);
  pass.last_material = material;
}

void Renderer::Impl::draw_primitive(const GpuPrimitive& primitive, const Matrix& transform, bool skinned,
                                    ObjectPass& pass) {
  use_material(primitive.material, pass);
  const bool two_sided = pass.last_two_sided;
  if (two_sided) rlDisableBackfaceCulling();
  // Per part: a skinned object may still have parts without bone weights, which stay rigid.
  const bool deformed = skinned && primitive.mesh.boneIds != nullptr;
  const float skinning = deformed ? kOn : kOff;
  rlSetUniform(pass.skinned_location, &skinning, SHADER_UNIFORM_FLOAT, 1);
  draw_mesh(primitive.mesh, transform, pass.shader, deformed ? &bone_matrices : nullptr);
  if (two_sided) rlEnableBackfaceCulling();
}

// --- Sky and particles ------------------------------------------------------------------------

void Renderer::Impl::draw_sky(const Camera3D& camera) {
  rlDisableBackfaceCulling();
  rlDisableDepthMask();
  rlDisableDepthTest();
  DrawMesh(sky_mesh, sky_material, MatrixTranslate(camera.position.x, camera.position.y, camera.position.z));
  rlEnableDepthTest();
  rlEnableDepthMask();
  rlEnableBackfaceCulling();
}

std::vector<const Texture2D*> Renderer::Impl::particle_pictures(const ParticleSystem& system) {
  std::vector<const Texture2D*> pictures;
  for (const std::string& name : system.images()) pictures.push_back(ui_image(name));
  return pictures;
}

void Renderer::Impl::draw_particle_batch(const ParticleSystem& system, const std::vector<const Texture2D*>& pictures,
                                         const Texture2D* picture, bool glowing, const render_math::Facing& view) {
  send(particle_shader, particle_textured_loc, picture != nullptr ? kOn : kOff);
  if (picture != nullptr) rlSetTexture(picture->id);
  rlBegin(RL_QUADS);
  for (const Particle& particle : system.particles()) {
    const Texture2D* worn =
        particle.image == kNoParticleImage ? nullptr : pictures[static_cast<std::size_t>(particle.image)];
    if (particle.glow == glowing && worn == picture) add_particle_quad(particle, view);
  }
  rlEnd();
  rlDrawRenderBatchActive();
  rlSetTexture(0);
}

void Renderer::Impl::draw_particles(const ParticleSystem& system, const Camera3D& camera) {
  if (system.particles().empty()) return;
  const render_math::Facing view =
      render_math::facing(to_vec3(camera.position), to_vec3(camera.target), to_vec3(camera.up));
  const std::vector<const Texture2D*> pictures = particle_pictures(system);
  const std::vector<const Texture2D*> batches = render_math::picture_batches(pictures);
  rlDrawRenderBatchActive();
  rlDisableDepthMask();
  rlDisableBackfaceCulling();
  BeginShaderMode(particle_shader);
  for (const bool glowing : {false, true}) {
    send(particle_shader, particle_boost_loc, glowing ? kParticleGlowBoost : kOn);
    blend_particles(glowing);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    for (const Texture2D* picture : batches) draw_particle_batch(system, pictures, picture, glowing, view);
    EndBlendMode();
  }
  EndShaderMode();
  rlEnableBackfaceCulling();
  rlEnableDepthMask();
}

}  // namespace redgem
