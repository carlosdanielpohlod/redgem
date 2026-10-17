#pragma once

// The renderer's insides: what it keeps on the graphics card and the steps of a frame. One
// struct, implemented part by part; only those parts include this:
//   renderer.cpp              the public interface
//   renderer_pipeline.cpp     what lives as long as the window: loading and releasing it
//   renderer_targets.cpp      what it draws into
//   renderer_assets.cpp       a scene's meshes, materials and textures
//   renderer_pictures.cpp     the font and the pictures games ask for by name
//   renderer_frame.cpp        a frame: its camera, what is prepared, the lit picture
//   renderer_frame_texels.cpp what the vertex shader reads of a frame: its matrices, its bones
//   renderer_environment.cpp  lights, atmosphere and the sky
//   renderer_objects.cpp      the scene's objects
//   renderer_scatter.cpp      copies scattered in great numbers
//   renderer_particles.cpp    particles
//   renderer_shadows.cpp      the sun's shadow map
//   renderer_reflections.cpp  the surroundings, for reflections
//   renderer_post.cpp         from the lit picture to the one on screen
//   renderer_overlay.cpp      what games draw over the picture
// This header stays long because it lists every step of those parts: they all work on the one
// renderer's state, which is declared once, here.

#include <raylib.h>
#include <rlgl.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/capture_views.hpp"
#include "render/environment.hpp"
#include "render/instance_batches.hpp"
#include "render/instance_records.hpp"
#include "render/mesh_pass.hpp"
#include "render/render_math.hpp"
#include "render/renderer.hpp"
#include "render/scatter_chunks.hpp"
#include "render/shader_programs.hpp"
#include "render/texel_rows.hpp"
#include "render/visibility.hpp"
#include "scene/scene_config.hpp"

namespace redgem {
namespace render_detail {

inline constexpr int kNoMaterialYet = -2;  // no material index (kNoIndex is the fallback material)
inline constexpr ::Color kWhite{config::kMaxColorByte, config::kMaxColorByte, config::kMaxColorByte,
                                config::kMaxColorByte};  // also the tint that leaves a drawn texture unchanged
inline constexpr ::Color kBlack{0, 0, 0, config::kMaxColorByte};
// Texture slots no material uses: bound once per frame.
inline constexpr int kReflectionTextureSlot = 9;
inline constexpr int kShadowTextureSlot = 10;
inline constexpr int kFrameTexelSlot = 11;

// raylib numbers what is on the graphics card; zero is what was never loaded (or is released).
inline bool loaded(const Texture2D& texture) { return texture.id != 0; }
inline bool loaded(const RenderTexture2D& target) { return target.id != 0; }
inline bool loaded(const Shader& shader) { return shader.id != 0; }

// One part of a mesh on the graphics card, and the material it is drawn with.
struct GpuPrimitive {
  Mesh mesh{};
  int material = kNoIndex;
  // Which of the mesh's pieces it is (see mesh_pieces): a piece that cannot be drawn has no part.
  std::size_t piece = 0;
  bool skinned = false;  // has joints: a skeleton deforms it
};

// What a frame knows of an object before any pass: whether it is drawn at all (its record is in
// the frame's texels), and what a pass judges it by (kept together, so that a pass over tens of
// thousands of objects reads them one after the other).
struct ObjectSight {
  bool drawn = false;
  render_math::Sight sight;
  int mesh = kNoIndex;
  float visible_from = 0.0f;
  float visible_to = 0.0f;
};

// What an object's record was last written from: most of a scene stands still, and a record
// written from the same is not worked out again.
struct RecordSource {
  Transform world;
  Vec3 tint;
  float first_bone = 0.0f;
  float record = 0.0f;  // its number
};

// Where a skeleton is this frame: its first bone (what a skinned mesh is judged by) and how many.
struct SkeletonPose {
  Matrix first{};
  std::size_t bones = 0;
};

// One pass over the scene's objects: what it is drawn with, what it can see, and the material
// the previous part left set (consecutive parts often share one).
struct ObjectPass {
  // cppcheck-suppress uninitMemberVarNoCtor ; it holds a reference: whoever makes one has to give it
  MeshPass meshes;
  int skinned_location = kNoLocation;
  bool depth = false;  // the shadow map's pass: only depth is written
  render_math::View view;
  int last_material = kNoMaterialYet;
  bool last_two_sided = false;
};

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

// Draws `source` over the whole current target (render textures are stored upside down).
void draw_full(const RenderTexture2D& source, int width, int height);

}  // namespace render_detail

struct Renderer::Impl {
  using GpuPrimitive = render_detail::GpuPrimitive;
  using ObjectPass = render_detail::ObjectPass;
  using ParticleDraw = render_detail::ParticleDraw;
  using ObjectSight = render_detail::ObjectSight;

  // Shaders, and where the ones drawn with all frame long take their values.
  Shader shader{};
  render_detail::LitUniforms lit_uniforms;
  Shader depth_shader{};
  render_detail::DepthUniforms depth_uniforms;
  int lit_instance_slot = render_detail::kNoLocation;  // the vertex attribute each copy's record number goes to
  int depth_instance_slot = render_detail::kNoLocation;
  Shader sky_shader{};
  Shader particle_shader{};
  render_detail::ParticleUniforms particle_uniforms;
  Shader panorama_shader{};
  Shader occlusion_shader{};
  Shader bright_shader{};
  Shader blur_shader{};
  Shader composite_shader{};
  Shader antialias_shader{};
  // Targets whose size does not follow the window.
  RenderTexture2D shadow{};           // depth of the scene seen from the sun (no color)
  RenderTexture2D reflection_face{};  // one 90-degree view of the surroundings
  RenderTexture2D panorama{};         // all of them, as longitude x latitude, with mips
  // Post-processing targets, sized like the output.
  RenderTexture2D hdr{};      // the lit scene, linear HDR, with depth
  RenderTexture2D bloom_a{};  // bright parts, blurred (ping-pong with bloom_b)
  RenderTexture2D bloom_b{};
  RenderTexture2D ldr{};          // tone mapped, luma in alpha, before anti-aliasing
  RenderTexture2D occlusion_a{};  // ambient occlusion (ping-pong with occlusion_b for the blur)
  RenderTexture2D occlusion_b{};
  // The scene's assets.
  std::vector<Texture2D> textures;
  std::vector<Material> materials;
  std::vector<MaterialDesc> material_factors;  // per material, read at draw time
  const MaterialDesc fallback_factors;         // of parts without a material: white dielectric
  Material fallback_material{};
  std::vector<std::vector<GpuPrimitive>> meshes;  // per MeshDesc
  std::vector<Bounds> mesh_bounds;                // per MeshDesc, in the mesh's own space
  std::vector<SkinDesc> skins;                    // skeletons of the uploaded scene
  Mesh sky_mesh{};
  Material sky_material{};
  // Texts and pictures.
  Font ui_font{};
  bool owns_ui_font = false;
  std::filesystem::path images_dir;
  std::unordered_map<std::string, Texture2D> ui_images;  // by name; id 0 = looked for and not found
  // What the game asked for.
  bool show_prefabs = false;
  float motion_blur = 0.0f;  // 0..1 (Camera.motion_blur)
  // The frame being drawn.
  std::vector<Transform> worlds;                            // every object's world transform (see prepare_frame)
  std::vector<ObjectSight> sights;                          // by object, for every pass of the frame
  std::vector<render_detail::RecordSource> record_sources;  // by object
  render_math::TexelRows texels;  // what the vertex shader reads of the frame, and its texture
  std::vector<render_math::RowRun> changed_rows;
  unsigned int texel_texture = 0;
  render_math::InstanceBatches batches;                     // what the pass being drawn sees, in runs of a mesh
  render_detail::SlotBuffer slots;                          // the record numbers of what the frame's passes see
  std::uint64_t scatter_version = 0;                        // of the scattered copies laid out (see Scattered)
  std::vector<render_math::ScatterLayout> scatter_layouts;  // whose records come first
  std::size_t scattered_records = 0;  // theirs come first in the frame's texels, the objects' after
  float wind = 0.0f;                  // of the environment
  Vector3 sun_travel{};               // direction the sun's light travels
  float view_tan_half_fov = 0.0f;     // of the camera, for rebuilding positions from depth
  float view_aspect = 0.0f;
  int next_face = 0;       // the cube face captured this frame
  int faces_captured = 0;  // reflections switch on once all six exist

  // --- What lives as long as the window (renderer_pipeline.cpp) ----------------------------------

  // Shaders, their uniforms, the shadow map, the reflection targets and the sky.
  void load_pipeline();
  void compile_shaders();
  void load_sky();
  void release();
  void release_sky();
  void release_shaders();

  // --- Targets (renderer_targets.cpp) -----------------------------------------------------------

  void load_fixed_targets();
  // The post-processing targets, made again when the output changes size.
  void ensure_targets(int width, int height);
  void release_targets();
  void release_fixed_targets();

  // --- The scene's assets (renderer_assets.cpp) -------------------------------------------------

  // Assets are only ever appended to a scene: these upload whatever is new.
  void upload_new_textures(const SceneDesc& desc);
  void upload_new_materials(const SceneDesc& desc);
  void upload_new_meshes(const SceneDesc& desc);
  // Every material's values as they are now (cheap: nothing goes to the graphics card).
  void refresh_material_values(const SceneDesc& desc);
  Material make_material(redgem::Color base_color) const;
  Material load_material(const MaterialDesc& source) const;
  void release_assets();
  void release_meshes();

  // --- Texts and pictures (renderer_pictures.cpp) -----------------------------------------------

  // The font of the game's texts: the interface font, rasterized large and scaled down smoothly.
  void load_ui_font();
  // The picture UI.image or a particle asks for by name, loaded on first use. Null (reported
  // once) when the game's images folder has no such file.
  const Texture2D* ui_image(const std::string& name);
  void release_ui_images();

  // --- A frame (renderer_frame.cpp) -------------------------------------------------------------

  Camera3D camera_for(const Scene& scene) const;
  // What every pass of a frame needs before the picture is drawn: where things are, the lights,
  // the sun's shadow map and one more face of the reflections. `aspect`: width over height.
  void prepare_frame(const Scene& scene, const Camera3D& camera, const Environment& env, float aspect);
  // The scene as the camera sees it, into the HDR target.
  void draw_lit_scene(const Camera3D& camera, const ParticleSystem* particles);
  // Binds a texture `program` reads all frame long to a slot of its own.
  void bind_for_the_frame(const Shader& program, int sampler_location, int slot, unsigned int texture);

  // --- What the vertex shader reads of a frame (renderer_frame_texels.cpp) ----------------------

  // Every object's record, sight and bones, for all the passes of the frame.
  void write_frame_texels(const Scene& scene);
  // One object's record and sight, and its bones from texel `next_bone` on. Returns where the
  // next skeleton's bones go.
  std::size_t sight_object(const Scene& scene, const GameObject& object, std::size_t next_bone);
  // Writes the record of the object in `slot`, unless it was written from the same last frame.
  bool write_record(std::size_t slot, const render_math::InstanceLook& look);
  // Writes an object's bones from texel `first` on: where each of its joints is now, times the
  // skin's inverse bind matrix. Nothing when the object has no (usable) skeleton.
  std::optional<render_detail::SkeletonPose> pose_skeleton(const Scene& scene, const GameObject& object,
                                                           std::size_t first);
  // The first `bones` bones of a skeleton, from texel `first` on; returns the first one's matrix.
  std::optional<Matrix> write_bones(const Scene& scene, const GameObject& object, const SkinDesc& skin,
                                    std::size_t first, std::size_t bones);
  std::optional<Matrix> write_bone(const Scene& scene, ObjectHandle joint,
                                   const std::array<float, kFloatsPerMatrix>& inverse_bind, std::size_t texel);
  // Hands what changed to the graphics card, and the texture to the shaders that read it.
  void send_frame_texels();
  void remake_texel_texture();
  void upload_changed_rows();

  // --- Lights, atmosphere and the sky (renderer_environment.cpp) --------------------------------

  // Sends the sun, the point lights nearest to the viewer and the environment to the shaders.
  void apply_lights(const Scene& scene, const Environment& env, Vec3 viewer);
  void send_lights(Vector3 sun_direction, Vector3 sun_color, float ambient,
                   const render_math::PointLightUniforms& points);
  void send_atmosphere(const Environment& env);
  void send_sky_dome(const Environment& env, Vector3 sun_direction, Vector3 sun_color);
  // A dome around the camera, behind everything (no depth written).
  void draw_sky(const Camera3D& camera);

  // --- Objects (renderer_objects.cpp) -----------------------------------------------------------

  const MaterialDesc& factors_of(int material) const;
  // Sets a material's uniforms (only cut-out and wind for the shadow depth pass). Returns true
  // when it must be drawn without backface culling.
  bool apply_material(int index, bool depth_pass);
  void send_surface(const MaterialDesc& factors);
  // Draws what the current 3D view can see, copies of a mesh that come together in one call per
  // part. `viewer`
  // is the game camera in both passes: detail is judged by how big things look to the player, not
  // to the sun.
  void draw_objects(bool depth_pass, Vec3 viewer, float detail_angle = 0.0f);
  // Gathers into `batches` what the view sees.
  void gather_copies(const render_math::View& view);
  // `first`: where the pass's record numbers start in `slots`.
  void draw_batch(const render_math::InstanceBatch& batch, std::size_t first, ObjectPass& pass);
  void use_material(int material, ObjectPass& pass);
  void draw_primitive(const GpuPrimitive& primitive, const render_detail::InstanceRun& run, ObjectPass& pass);

  // --- Scattered copies (renderer_scatter.cpp) -------------------------------------------------

  // Writes the records of the scene's scattered copies, cell by cell, when they changed.
  void lay_out_scattered(const Scattered& scattered);
  // One set's, from record `first` on. Returns the record after its last.
  std::size_t lay_out_set(const ScatterSet& set, std::size_t first);
  // Adds to `batches` the cells the view sees.
  void gather_scattered(const render_math::View& view);

  // --- Particles (renderer_particles.cpp) -------------------------------------------------------

  // Particles are hidden behind objects but do not hide each other (no depth written). Ordinary
  // ones cover what is behind them; glowing ones add their light to it.
  void draw_particles(const ParticleSystem& system, const Camera3D& camera);
  // The particles that glow, or the ones that do not: each kind mixes with the picture in its own way.
  void draw_particle_layer(const ParticleDraw& draw, bool glowing);
  // The pictures a particle system uses, by Particle::image. Null for one the game's images
  // folder does not have: those particles are drawn as dots.
  std::vector<const Texture2D*> particle_pictures(const ParticleSystem& system);
  // Draws the particles that glow (or the ones that do not) and wear `picture` (null: plain dots).
  void draw_particle_batch(const ParticleDraw& draw, const Texture2D* picture, bool glowing);

  // --- Sun shadows (renderer_shadows.cpp) -------------------------------------------------------

  // Renders the scene's depth from the sun and hands the result to the lit shader.
  void draw_shadow_map(const Camera3D& camera, const Environment& env);
  void draw_sun_depth(const Camera3D& camera, Vec3 to_sun);
  // Fills the shadow map from the sun's point of view. Returns the matrix that leads into that view.
  Matrix draw_depth_from(const Camera3D& sun_view, Vec3 viewer);

  // --- Reflections (renderer_reflections.cpp) ---------------------------------------------------

  // Captures one more face of the camera's surroundings into the panorama the lit shader reflects.
  // Six frames make a full turn, so reflections follow the camera with a short delay.
  void capture_reflection_face(const Camera3D& camera);
  void draw_surroundings(const render_math::CubeFace& face, Vector3 eye);
  void copy_face_to_panorama(const render_math::CubeFace& face);
  void blur_panorama_levels();

  // --- Post-processing (renderer_post.cpp) ------------------------------------------------------

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

  // --- Over the picture (renderer_overlay.cpp) --------------------------------------------------

  void draw_hud_text(const HudItem& item);
  void draw_hud_image(const HudItem& item);
};

}  // namespace redgem
