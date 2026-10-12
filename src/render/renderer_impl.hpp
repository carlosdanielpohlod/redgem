#pragma once

// The renderer's insides: what it keeps on the graphics card and the steps of a frame. Only the
// renderer's own source files include this:
//   renderer.cpp         the public interface, and what games draw over the picture
//   renderer_setup.cpp   loading and releasing shaders, targets, meshes, textures, pictures
//   renderer_scene.cpp   drawing the scene's objects, sky and particles
//   renderer_passes.cpp  the passes around them: shadows, reflections, post-processing

#include <raylib.h>
#include <rlgl.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "scene/scene_config.hpp"
#include "render/environment.hpp"
#include "render/render_math.hpp"
#include "render/renderer.hpp"

namespace redgem {
namespace render_detail {

inline constexpr int kNoLocation = -1;     // a uniform the shader does not have
inline constexpr int kNoMaterialYet = -2;  // no material index (kNoIndex is the fallback material)
inline constexpr ::Color kWhite{config::kMaxColorByte, config::kMaxColorByte, config::kMaxColorByte,
                                config::kMaxColorByte};  // also the tint that leaves a drawn texture unchanged
inline constexpr ::Color kBlack{0, 0, 0, config::kMaxColorByte};
// Texture slots no material uses: bound once per frame.
inline constexpr int kReflectionTextureSlot = 9;
inline constexpr int kShadowTextureSlot = 10;
inline constexpr int kShadowMapSize = render_math::kShadowMapSize;
// The texture maps materials use (base color, emission, normal, metallic-roughness, occlusion).
inline constexpr int kMaterialMaps[] = {MATERIAL_MAP_ALBEDO, MATERIAL_MAP_EMISSION, MATERIAL_MAP_NORMAL,
                                        MATERIAL_MAP_ROUGHNESS, MATERIAL_MAP_OCCLUSION};

// One part of a mesh on the graphics card, and the material it is drawn with.
struct GpuPrimitive {
  Mesh mesh{};
  int material = kNoIndex;
};

// One pass over the scene's objects: what it is drawn with, what it can see, and the material
// the previous part left set (consecutive parts often share one).
struct ObjectPass {
  // cppcheck-suppress uninitMemberVarNoCtor ; a reference: whoever makes one has to give it
  const Shader& shader;
  int skinned_location = kNoLocation;
  bool depth = false;  // the shadow map's pass: only depth is written
  render_math::View view;
  int last_material = kNoMaterialYet;
  bool last_two_sided = false;
};

// Sets one uniform of a shader, by location. (A shader without that uniform is left alone.)
inline void send(const Shader& shader, int location, float value) {
  SetShaderValue(shader, location, &value, SHADER_UNIFORM_FLOAT);
}
inline void send(const Shader& shader, int location, int value) {
  SetShaderValue(shader, location, &value, SHADER_UNIFORM_INT);
}
inline void send(const Shader& shader, int location, Vector2 value) {
  SetShaderValue(shader, location, &value, SHADER_UNIFORM_VEC2);
}
inline void send(const Shader& shader, int location, Vector3 value) {
  SetShaderValue(shader, location, &value, SHADER_UNIFORM_VEC3);
}
// The same, by name: for uniforms set too seldom to be worth remembering their location.
template <typename Value>
void send(const Shader& shader, const char* name, Value value) {
  send(shader, GetShaderLocation(shader, name), value);
}

}  // namespace render_detail

struct Renderer::Impl {
  using GpuPrimitive = render_detail::GpuPrimitive;
  using ObjectPass = render_detail::ObjectPass;

  Shader shader{};
  Shader depth_shader{};
  Shader sky_shader{};
  Shader particle_shader{};
  int particle_boost_loc = render_detail::kNoLocation;
  int particle_textured_loc = render_detail::kNoLocation;
  Shader panorama_shader{};
  Shader occlusion_shader{};
  RenderTexture2D occlusion_a{};  // ambient occlusion (ping-pong with occlusion_b for the blur)
  RenderTexture2D occlusion_b{};
  float view_tan_half_fov = 0.0f;  // of the camera being drawn, for rebuilding positions from depth
  float view_aspect = 0.0f;
  RenderTexture2D reflection_face{};  // one 90-degree view of the surroundings
  RenderTexture2D panorama{};         // all of them, as longitude x latitude, with mips
  int next_face = 0;                  // the cube face captured this frame
  int faces_captured = 0;             // reflections switch on once all six exist
  int reflection_map_loc = 0;
  int reflection_strength_loc = 0;
  Shader bright_shader{};
  Shader blur_shader{};
  Shader composite_shader{};
  Shader antialias_shader{};
  // Lit shader uniforms.
  int sun_direction_loc = 0;
  int sun_color_loc = 0;
  int ambient_loc = 0;
  int point_count_loc = 0;
  int point_positions_loc = 0;
  int point_colors_loc = 0;
  int point_ranges_loc = 0;
  int fog_color_loc = 0;
  int fog_density_loc = 0;
  int metallic_loc = 0;
  int roughness_loc = 0;
  int emissive_loc = 0;
  int has_emissive_map_loc = 0;
  int has_normal_map_loc = 0;
  int normal_scale_loc = 0;
  int has_surface_map_loc = 0;
  int occlusion_strength_loc = 0;
  int shadow_map_loc = 0;
  int light_matrix_loc = 0;
  int shadow_strength_loc = 0;
  int shadow_texel_loc = 0;
  int depth_alpha_cutoff_loc = 0;
  int depth_wind_loc = 0;
  int depth_time_loc = 0;
  int alpha_cutoff_loc = 0;
  int tint_loc = 0;
  int wind_loc = 0;
  int time_loc = 0;
  int sky_top_loc = 0;
  int sky_horizon_loc = 0;
  int ground_loc = 0;
  std::vector<MaterialDesc> material_factors;  // per material, read at draw time
  const MaterialDesc fallback_factors;         // of parts without a material: white dielectric
  bool show_prefabs = false;
  std::vector<std::vector<GpuPrimitive>> meshes;  // per MeshDesc
  std::vector<Bounds> mesh_bounds;                // per MeshDesc, in the mesh's own space
  std::vector<SkinDesc> skins;                    // skeletons of the uploaded scene
  std::vector<Matrix> bone_matrices;              // of the skinned object being drawn
  int skinned_loc = 0;
  int depth_skinned_loc = 0;
  std::vector<Material> materials;
  Material fallback_material{};
  std::vector<Texture2D> textures;
  Mesh sky_mesh{};
  Material sky_material{};
  // Post-processing targets, sized like the output.
  RenderTexture2D hdr{};          // the lit scene, linear HDR, with depth
  float motion_blur = 0.0f;       // 0..1, asked for by the game (Camera.motion_blur)
  Matrix pass_view_projection{};  // of the pass being drawn (see begin_pass)
  std::vector<Transform> worlds;  // every object's world transform this frame (see place_objects)
  Font ui_font{};
  bool owns_ui_font = false;
  std::filesystem::path images_dir;
  std::unordered_map<std::string, Texture2D> ui_images;  // by name; id 0 = looked for and not found
  RenderTexture2D bloom_a{};                             // bright parts, blurred (ping-pong with bloom_b)
  RenderTexture2D bloom_b{};
  RenderTexture2D ldr{};     // tone mapped, luma in alpha, before anti-aliasing
  RenderTexture2D shadow{};  // depth of the scene seen from the sun (no color)
  float wind = 0.0f;         // of the environment being drawn
  Vector3 sun_travel{};      // direction the sun's light travels this frame

  // --- Loading and releasing (renderer_setup.cpp) -----------------------------------------------

  static Shader compile(const char* vertex, const char* fragment, const char* name);
  // Everything that lives as long as the window: shaders, their uniforms, the shadow map, the
  // reflection targets and the sky.
  void load_pipeline();
  void compile_shaders();
  // A uniform of the lit shader, and the member that keeps its location.
  struct Uniform {
    int Impl::* location;
    const char* name;
  };
  static const Uniform kMaterialUniforms[];
  static const Uniform kFrameUniforms[];
  void find_material_uniforms();
  void find_light_uniforms();
  void find_depth_and_particle_uniforms();
  void load_fixed_targets();
  void load_sky();
  Material make_material(redgem::Color base_color);
  void release();
  void release_assets();
  void release_meshes();
  void release_sky();
  void release_fixed_targets();
  void release_shaders();
  void release_targets();
  void release_ui_images();

  static RenderTexture2D load_target(int width, int height, int format, bool with_depth);
  static Texture2D load_color_texture(unsigned int framebuffer, int width, int height, int format);
  static Texture2D load_depth_texture(unsigned int framebuffer, int width, int height);
  // A framebuffer with only a depth texture, which the lit shader samples.
  static RenderTexture2D load_shadow_target(int size);
  void ensure_targets(int width, int height);

  // Assets are only ever appended to a scene: these upload whatever is new.
  void upload_new_textures(const SceneDesc& desc);
  void upload_new_materials(const SceneDesc& desc);
  void upload_new_meshes(const SceneDesc& desc);
  // Every material's values as they are now (cheap: nothing goes to the graphics card).
  void refresh_material_values(const SceneDesc& desc);
  static Texture2D load_texture(const ImageData& image);
  Material load_material(const MaterialDesc& source);
  static std::vector<GpuPrimitive> upload_mesh(const MeshDesc& mesh_desc);

  // The font of the game's texts: the interface font, rasterized large and scaled down smoothly.
  void load_ui_font();
  // The picture UI.image or a particle asks for by name, loaded on first use. Null (reported
  // once) when the game's images folder has no such file.
  const Texture2D* ui_image(const std::string& name);
  Texture2D load_picture(const std::string& name) const;

  // --- The scene (renderer_scene.cpp) -----------------------------------------------------------

  Camera3D camera_for(const Scene& scene) const;
  // Sends the sun, the point lights nearest to the viewer and the environment to the shaders.
  void apply_lights(const Scene& scene, const Environment& env, Vec3 viewer);
  void send_lights(Vector3 sun_direction, Vector3 sun_color, float ambient,
                   const render_math::PointLightUniforms& points);
  void send_atmosphere(const Environment& env);
  void send_sky_dome(const Environment& env, Vector3 sun_direction, Vector3 sun_color);
  // Where every object is in the world this frame, worked out once: each of the frame's passes
  // (shadows, reflections, the view) would otherwise walk every object's chain of parents again.
  void place_objects(const Scene& scene);
  // Fills bone_matrices for an object's skeleton: where each of its joints is now, times the
  // skin's inverse bind matrix. False when the object has no (usable) skeleton.
  bool pose_skeleton(const Scene& scene, const GameObject& object);

  const MaterialDesc& factors_of(int material) const;
  // Sets a material's uniforms (only cut-out and wind for the shadow depth pass). Returns true
  // when it must be drawn without backface culling.
  bool apply_material(int index, bool depth_pass);
  void send_surface(const MaterialDesc& factors);

  void begin_pass(const Shader& pass_shader);
  void bind_material(const Material& material, const Shader& pass_shader);
  void send_matrices(const Matrix& model, const Shader& pass_shader);
  // `bones` is null for meshes that are not deformed by a skeleton.
  void draw_mesh(const Mesh& mesh, const Matrix& model, const Shader& pass_shader, const std::vector<Matrix>* bones);
  // Leaves the state raylib expects: nothing bound.
  void end_pass();

  // Draws what the current 3D view can see. `viewer` is the game camera in both passes: detail
  // is judged by how big things look to the player, not to the sun.
  void draw_objects(const Scene& scene, bool depth_pass, Vec3 viewer, float detail_angle = 0.0f);
  void draw_object(const Scene& scene, const GameObject& object, const Transform& world, ObjectPass& pass);
  bool in_view(const GameObject& object, const Transform& world, bool skinned, const render_math::View& view) const;
  void use_material(int material, ObjectPass& pass);
  void draw_primitive(const GpuPrimitive& primitive, const Matrix& transform, bool skinned, ObjectPass& pass);

  // A dome around the camera, behind everything (no depth written).
  void draw_sky(const Camera3D& camera);
  // Particles are hidden behind objects but do not hide each other (no depth written). Ordinary
  // ones cover what is behind them; glowing ones add their light to it.
  void draw_particles(const ParticleSystem& system, const Camera3D& camera);
  // What every layer of particles is drawn with: their pictures by Particle::image, the distinct
  // ones among those (one batch each), and how the camera faces them.
  struct ParticleDraw {
    const ParticleSystem& system;
    // cppcheck-suppress uninitMemberVarNoCtor ; a reference: whoever makes one has to give it
    const std::vector<const Texture2D*>& pictures;
    // cppcheck-suppress uninitMemberVarNoCtor ; a reference: whoever makes one has to give it
    const std::vector<const Texture2D*>& batches;
    const render_math::Facing& view;
  };
  // The particles that glow, or the ones that do not: each kind mixes with the picture in its own way.
  void draw_particle_layer(const ParticleDraw& draw, bool glowing);
  // The pictures a particle system uses, by Particle::image. Null for one the game's images
  // folder does not have: those particles are drawn as dots.
  std::vector<const Texture2D*> particle_pictures(const ParticleSystem& system);
  // Draws the particles that glow (or the ones that do not) and wear `picture` (null: plain dots).
  void draw_particle_batch(const ParticleSystem& system, const std::vector<const Texture2D*>& pictures,
                           const Texture2D* picture, bool glowing, const render_math::Facing& view);

  // What every pass of a frame needs before the picture is drawn: where things are, the lights,
  // the sun's shadow map and one more face of the reflections. `aspect`: width over height.
  void prepare_frame(const Scene& scene, const Camera3D& camera, const Environment& env, float aspect);
  // The scene as the camera sees it, into the HDR target.
  void draw_lit_scene(const Scene& scene, const Camera3D& camera, const ParticleSystem* particles);

  // --- The passes around it (renderer_passes.cpp) -----------------------------------------------

  // Binds a texture the lit shader reads all frame long to a slot of its own.
  void bind_for_the_frame(int sampler_location, int slot, unsigned int texture);

  // Renders the scene's depth from the sun and hands the result to the lit shader.
  void draw_shadow_map(const Scene& scene, const Camera3D& camera, const Environment& env);
  void draw_sun_depth(const Scene& scene, const Camera3D& camera, Vec3 to_sun);
  // Fills the shadow map from the sun's point of view. Returns the matrix that leads into that view.
  Matrix draw_depth_from(const Camera3D& sun_view, const Scene& scene, Vec3 viewer);

  // Captures one more face of the camera's surroundings into the panorama the lit shader reflects.
  // Six frames make a full turn, so reflections follow the camera with a short delay.
  void capture_reflection_face(const Scene& scene, const Camera3D& camera);
  void draw_surroundings(const Scene& scene, const render_math::CubeFace& face, Vector3 eye);
  void copy_face_to_panorama(const render_math::CubeFace& face);
  void blur_panorama_levels();

  // Draws `source` over the whole current target (render textures are stored upside down).
  static void draw_full(const RenderTexture2D& source, int width, int height);
  // Draws `source` through a full-screen shader into `target`.
  void apply_effect(const RenderTexture2D& target, Shader& effect, const RenderTexture2D& source);
  // hdr -> bloom -> tone mapping -> anti-aliasing -> `output` (the screen when null).
  void post_process(const Environment& env, const RenderTexture2D* output);
  void draw_occlusion();
  void draw_bloom();
  // Blurs `picture` across, then down. `scratch` (the same size) holds the half-way result.
  void blur(const RenderTexture2D& picture, const RenderTexture2D& scratch);
  void tone_map(const Environment& env);
  void send_grade(const Environment& env);
  void antialias(const RenderTexture2D* output);
  void copy_depth_to(const RenderTexture2D& output);

  // --- Over the picture (renderer.cpp) ----------------------------------------------------------

  void draw_hud_text(const HudItem& item);
  void draw_hud_image(const HudItem& item);
};

}  // namespace redgem
