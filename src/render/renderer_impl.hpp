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
//   renderer_sky.cpp          the sky as a picture, and its light
//   renderer_objects.cpp      the scene's objects
//   renderer_see_through.cpp  their see-through parts, after the opaque ones
//   renderer_scatter.cpp      copies scattered in great numbers
//   renderer_particles.cpp    particles
//   renderer_particle_glow.cpp glowing particles held to the environment's limit
//   renderer_temporal.cpp     edges smoothed with the frames before (temporal anti-aliasing)
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
#include "render/gl_extras.hpp"
#include "render/instance_batches.hpp"
#include "render/instance_records.hpp"
#include "render/material_alpha.hpp"
#include "render/mesh_pass.hpp"
#include "render/particle_buffer.hpp"
#include "render/particle_layout.hpp"
#include "render/render_math.hpp"
#include "render/renderer.hpp"
#include "render/scatter_chunks.hpp"
#include "render/shader_programs.hpp"
#include "render/shaders/lit_fragment.hpp"
#include "render/shadow_cascades.hpp"
#include "render/sky_harmonics.hpp"
#include "render/temporal.hpp"
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
inline constexpr int kSkyReflectionSlot = 12;
inline constexpr int kSkyReflectanceSlot = 13;
inline constexpr int kSceneDepthSlot = 14;     // what soft particles fade into
inline constexpr int kPreviousTexelSlot = 15;  // the last frame's records and bones (temporal anti-aliasing)

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
  bool cast_shadow = true;
};

// Whether what casts a shadow or not (`cast_shadow`) is drawn in a pass: the shadow map's pass
// (`depth_pass`) leaves out what casts none.
inline bool casts_into(bool cast_shadow, bool depth_pass) { return cast_shadow || !depth_pass; }

// What an object's record was last written from: most of a scene stands still, and a record
// written from the same is not worked out again.
struct RecordSource {
  Transform world;
  Vec3 tint;
  float first_bone = 0.0f;
  float sway = 0.0f;
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
  render_math::AlphaPass alpha = render_math::AlphaPass::Opaque;  // Shadow: only depth is written
  render_math::View view;
  int last_material = kNoMaterialYet;
  bool last_two_sided = false;
};

// A sky picture on the graphics card, and its light (see sky_light.hpp).
struct SkyTextures {
  std::string name;  // what the environment asked for, looked for once ("": none)
  Texture2D picture{};
  unsigned int reflections = 0;  // a cube map, a roughness a level
  unsigned int reflectance = 0;  // the split sum's table: made once, kept for the next picture
  render_math::SkyHarmonics irradiance{};
};

inline constexpr std::size_t kHistories = 2;  // the mixed frames: this one's, and the last one's

// Temporal anti-aliasing's state from frame to frame (see temporal.hpp).
struct TemporalState {
  bool on = false;             // this frame is mixed with the ones before
  bool history_valid = false;  // ... and they can be used
  bool seen_before = false;    // `view` is the last frame's: it was mixed too
  std::uint64_t frame = 0;     // frames mixed so far: the jitter's phase
  std::size_t latest = 0;      // which of the histories holds the newest mix
  render_math::TemporalView view;
  Matrix view_projection{};           // this frame's camera, without the jitter
  Matrix previous_view_projection{};  // the last frame's
  float time = 0.0f;                  // the wind's clock, this frame and the last
  float previous_time = 0.0f;
};

// The last frame's records and bones, kept on the graphics card for the motion pass, and the
// framebuffers they are copied through.
struct PreviousTexels {
  unsigned int texture = 0;
  int rows = 0;
  unsigned int from = 0;  // a framebuffer of the frame's texels
  unsigned int to = 0;    // one of `texture`
};

// One texel of a texture `pixels` wide, in texture coordinates.
inline float texel_of(int pixels) { return 1.0f / static_cast<float>(pixels); }
// Draws `source` over the whole current target (render textures are stored upside down).
void draw_full(const RenderTexture2D& source, int width, int height);

}  // namespace render_detail

struct Renderer::Impl {
  using GpuPrimitive = render_detail::GpuPrimitive;
  using ObjectPass = render_detail::ObjectPass;
  using ObjectSight = render_detail::ObjectSight;

  // Shaders, and where the ones drawn with all frame long take their values.
  Shader shader{};
  render_detail::LitUniforms lit_uniforms;
  shaders::LitFeatures lit_features;  // what the lit shader was compiled with
  Shader depth_shader{};
  render_detail::DepthUniforms depth_uniforms;
  int lit_instance_slot = render_detail::kNoLocation;  // the vertex attribute each copy's record number goes to
  int depth_instance_slot = render_detail::kNoLocation;
  Shader sky_shader{};
  Shader particle_shader{};
  render_detail::ParticleUniforms particle_uniforms;
  Shader glow_limit_shader{};
  Shader panorama_shader{};
  Shader occlusion_shader{};
  Shader bright_shader{};
  Shader blur_shader{};
  Shader composite_shader{};
  Shader antialias_shader{};
  Shader motion_shader{};  // temporal anti-aliasing: where things were, mixing the frames, sharpening
  render_detail::MotionUniforms motion_uniforms;
  int motion_instance_slot = render_detail::kNoLocation;
  Shader temporal_shader{};
  render_detail::TemporalUniforms temporal_uniforms;
  Shader sharpen_shader{};
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
  render_detail::Multisampled scene_samples;  // the lit scene of several samples a pixel (msaa), resolved into hdr
  bool samples_unusable = false;              // the card could not copy them into hdr: one sample a pixel from then on
  bool covering = false;                      // the pass being drawn smooths cut-out edges with those samples
  // The depth of the scene's surfaces, copied for soft particles (no color): made when first needed.
  RenderTexture2D scene_depth{};
  // Glowing particles gathered apart, to be added within the environment's limit: made when first
  // needed, on the lit picture's own depth.
  RenderTexture2D glow{};
  // Temporal anti-aliasing's, made when a scene first asks for it: where each pixel was the frame
  // before (with the depth of the surfaces it was drawn for), and the mixed frames, this one's and
  // the last one's in turn.
  RenderTexture2D motion{};
  std::array<RenderTexture2D, render_detail::kHistories> histories{};
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
  Texture2D sky_default_texture{};  // what the sky material had: drawn with when there is no picture
  render_detail::SkyTextures sky;
  // Texts and pictures.
  Font ui_font{};
  bool owns_ui_font = false;
  std::filesystem::path images_dir;
  std::unordered_map<std::string, Texture2D> ui_images;  // by name; id 0 = looked for and not found
  // What the game asked for.
  bool show_prefabs = false;
  std::uint64_t scene_uploads = 0;  // scenes uploaded so far: what a frame was drawn of
  float motion_blur = 0.0f;         // 0..1 (Camera.motion_blur)
  std::optional<double> clock;      // seconds (Renderer::set_clock); nothing: the real time
  float now() const { return static_cast<float>(clock ? *clock : GetTime()); }
  // The frame being drawn.
  std::vector<Transform> worlds;                            // every object's world transform (see prepare_frame)
  std::vector<ObjectSight> sights;                          // by object, for every pass of the frame
  std::vector<render_detail::RecordSource> record_sources;  // by object
  render_math::TexelRows texels;  // what the vertex shader reads of the frame, and its texture
  std::vector<render_math::RowRun> changed_rows;
  unsigned int texel_texture = 0;
  int texel_texture_rows = 0;
  render_detail::PreviousTexels previous_texels;
  // Records and bones that were another object's (or nobody's) the frame before: the last frame's
  // copy of them is this frame's, so that they show no motion they did not make. By texel ranges.
  std::vector<std::pair<std::size_t, std::size_t>> fresh_texels;
  std::vector<ObjectHandle> record_owners;  // by object slot: whose record it held last frame
  bool temporal_allowed = true;             // Renderer::allow_temporal_antialias
  bool temporal_unsupported = false;        // the card cannot copy the frame's float texels
  render_detail::TemporalState temporal;
  render_math::InstanceBatches batches;                     // what the pass being drawn sees, in runs of a mesh
  render_detail::SlotBuffer slots;                          // the record numbers of what the frame's passes see
  std::uint64_t scatter_version = 0;                        // of the scattered copies laid out (see Scattered)
  std::vector<render_math::ScatterLayout> scatter_layouts;  // whose records come first
  std::size_t scattered_records = 0;  // theirs come first in the frame's texels, the objects' after
  float wind = 0.0f;                  // of the environment
  float glow_limit = 0.0f;            // of the environment (0: glowing particles are not gathered apart)
  bool glow_gathered = false;         // this frame's glowing particles wait in `glow`
  Vector3 sun_travel{};               // direction the sun's light travels
  std::array<Matrix, render_math::kMaxShadowCascades> light_matrices{};  // into each cascade's view
  float view_tan_half_fov = 0.0f;  // of the camera, for rebuilding positions from depth
  float view_aspect = 0.0f;
  // The frame's particles: their pictures (by Particle::look.image), the distinct ones among those
  // (a batch each) and each picture's batch, how they are laid out, and that on the graphics card.
  std::vector<const Texture2D*> particle_pictures;
  std::vector<const Texture2D*> particle_batches;
  std::vector<std::size_t> particle_batch_of;
  render_math::ParticleLayout particle_layout;
  render_detail::ParticleBuffer particle_buffer;
  std::vector<render_math::SeeThroughDraw> see_through;  // the frame's, in the order they are drawn
  std::vector<bool> see_through_meshes;                  // by mesh: whether a part of it is drawn see-through
  bool any_see_through = false;                          // of any mesh: when none is, nothing is gathered
  int next_face = 0;                                     // the cube face captured this frame
  int faces_captured = 0;                                // reflections switch on once all six exist

  // --- What lives as long as the window (renderer_pipeline.cpp) ----------------------------------

  // Shaders, their uniforms, the shadow map, the reflection targets and the sky.
  void load_pipeline();
  void compile_shaders();
  // The lit shader with these features (compiled again only when they change: the sky picture or
  // soft shadows switched on or off).
  void compile_lit(const shaders::LitFeatures& features);
  void use_lit_features(const shaders::LitFeatures& features);
  void load_sky();
  void release();
  void release_sky();
  void release_shaders();

  // --- Targets (renderer_targets.cpp) -----------------------------------------------------------

  void load_fixed_targets();
  // The post-processing targets, made again when the output changes size, and the scene's picture
  // of `samples` a pixel (none for 0).
  void ensure_targets(int width, int height, int samples);
  void make_targets(int width, int height);
  // Copies the picture of several samples a pixel into hdr; when the card cannot, says so once and
  // draws one sample a pixel from then on.
  void resolve_samples();
  void ensure_glow_target();
  void ensure_scene_depth_target();
  void ensure_temporal_targets();
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
  // What the environment asks of the frame's shaders: the sky picture, the lit shader's features,
  // smoothing with the frames before.
  void use_environment(const Environment& env, const Camera3D& camera);
  // The opaque surfaces (with several samples a pixel when `sampled`), where they moved from, and
  // the see-through ones.
  void draw_surfaces(Vec3 viewer, bool sampled);
  void draw_scene_particles(const ParticleSystem* particles, const Camera3D& camera);
  // Binds a texture `program` reads all frame long to a slot of its own.
  void bind_for_the_frame(const Shader& program, int sampler_location, int slot, unsigned int texture);
  void bind_cube_for_the_frame(const Shader& program, int sampler_location, int slot, unsigned int texture);

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

  // --- The sky as a picture (renderer_sky.cpp) --------------------------------------------------

  // Loads the picture the environment names the first time it names it (and lets the last one go).
  void use_sky_picture(const Environment& env);
  void upload_sky_picture(const FloatImage& picture);
  void release_sky_picture();
  // Tells the lit and sky shaders whether the sky is a picture, how it is turned and its light.
  void send_sky_light(const Environment& env);
  void release_sky_light();

  // --- Objects (renderer_objects.cpp) -----------------------------------------------------------

  const MaterialDesc& factors_of(int material) const;
  // Sets a material's uniforms for a pass (only cut-out and wind for the shadow map's). Returns
  // true when it must be drawn without backface culling.
  bool apply_material(int index, render_math::AlphaPass pass);
  void send_surface(const MaterialDesc& factors);
  void cover_edges(bool cut_out);
  void send_alpha(const render_math::AlphaTest& test);
  void send_motion_alpha(const render_math::AlphaTest& test, bool covering);
  // Draws what the current 3D view can see, copies of a mesh that come together in one call per
  // part. `viewer` is the game camera in both passes: detail is judged by how big things look to
  // the player, not to the sun. Returns where the pass's record numbers start in `slots`.
  std::size_t draw_objects(bool depth_pass, Vec3 viewer, float detail_angle = 0.0f);
  // Gathers into `batches` what the view sees (in the shadow map's pass, what casts a shadow).
  void gather_copies(const render_math::View& view, bool depth_pass);
  // `first`: where the pass's record numbers start in `slots`.
  void draw_batch(const render_math::InstanceBatch& batch, std::size_t first, ObjectPass& pass);
  void use_material(int material, ObjectPass& pass);
  void draw_primitive(const GpuPrimitive& primitive, const render_detail::InstanceRun& run, ObjectPass& pass);

  // --- See-through surfaces (renderer_see_through.cpp) -------------------------------------------

  // After the opaque surfaces of the view whose records start at `first` in `slots`: the
  // see-through parts of what it saw, the farthest first, mixed with what is behind them.
  void draw_see_through(std::size_t first, Vec3 viewer);
  void gather_see_through(std::size_t first, Vec3 viewer);
  void add_see_through(const render_math::InstanceBatch& batch, std::size_t first, Vec3 viewer);
  bool has_see_through(int mesh) const;
  // Which meshes have see-through parts, worked out whenever meshes or materials are uploaded.
  void note_see_through_meshes();
  void draw_see_through_copies(const render_math::SeeThroughDraw& draw, ObjectPass& pass);

  // --- Scattered copies (renderer_scatter.cpp) -------------------------------------------------

  // Writes the records of the scene's scattered copies, cell by cell, when they changed.
  void lay_out_scattered(const Scattered& scattered);
  // One set's, from record `first` on. Returns the record after its last.
  std::size_t lay_out_set(const ScatterSet& set, std::size_t first);
  // Adds to `batches` the cells the view sees.
  void gather_scattered(const render_math::View& view, bool depth_pass);

  // --- Particles (renderer_particles.cpp) -------------------------------------------------------

  // Particles are hidden behind objects but do not hide each other (no depth written). Ordinary
  // ones cover what is behind them; glowing ones add their light to it.
  void draw_particles(const ParticleSystem& system, const Camera3D& camera);
  // The pictures a particle system uses and their batches. A picture the game's images folder does
  // not have is null: those particles are drawn as dots.
  void batch_particle_pictures(const ParticleSystem& system);
  // Lays the particles out, sends them to the graphics card, and copies what soft ones fade into.
  void prepare_particles(const ParticleSystem& system, const Camera3D& camera);
  // What every run of particles is drawn with: the shader, its view, and what soft ones fade into.
  void begin_particles();
  void draw_particle_runs(bool glowing);
  void draw_particle_run(const render_math::ParticleRun& run);
  // A run's picture and look, drawn as the blending set says.
  void draw_particle_picture(const render_math::ParticleRun& run);
  void end_particles();
  // Glowing particles, when the environment limits them, are gathered apart (into `glow`) and added
  // to the picture within that limit once every particle is drawn.
  bool gathers_glow() const { return glow_limit > 0.0f && particle_layout.any_glow; }
  void begin_gathering_glow();
  void add_gathered_glow();
  // The framebuffer the lit scene is being drawn into: of several samples a pixel, or the lit picture.
  unsigned int drawing_framebuffer() const;
  // The depth of what was drawn so far where particles read it: in the lit picture's own (which
  // glow gathered apart is hidden by), and copied for soft particles to fade into.
  void share_scene_depth();
  // The depth soft particles fade into, copied from the lit picture's.
  void copy_soft_particle_depth();
  // The light lit particles take: the sun, the sky and the nearest point lights.
  void send_particle_lights(Vector3 sun_color, const Environment& env, const render_math::PointLightUniforms& points);

  // --- Temporal anti-aliasing (renderer_temporal.cpp) -------------------------------------------

  // Whether the environment asks for it, and whether the frames before can be used (see
  // temporal.hpp); the shift of this frame's fine patterns.
  void begin_temporal(const Environment& env, const Camera3D& camera);
  render_math::TemporalView temporal_view(const Camera3D& camera) const;
  // Moves the view of the lit pass (between BeginMode3D and EndMode3D) by this frame's jitter,
  // keeping the view without it.
  void jitter_view();
  bool motion_needed() const { return temporal.on && temporal.history_valid; }
  // Where the opaque surfaces of the lit pass (their record numbers from `first` in `slots`) moved
  // from, into `motion`; drawing goes on where it was.
  void draw_motion(std::size_t first, Vec3 viewer);
  void send_motion_frame(Vec3 viewer);
  // Where this frame's particles and see-through surfaces (which write no motion: what is behind
  // them shows through) cover the picture, into the motion target's alpha.
  void mark_particle_cover();
  void mark_see_through_cover(Vec3 viewer);
  // The frame's texels as they were, before this frame's go to the graphics card.
  void keep_previous_texels();
  // Notes when an object's record (and bones) were not its own the frame before.
  void note_fresh_record(const Scene& scene, const GameObject& object, float first_bone, std::size_t bones);
  void carry_fresh_texels();
  void give_up_temporal();
  void release_temporal_targets();
  void remake_previous_texels();
  void release_previous_texels();
  // The lit picture mixed with the frames before, into the newer history.
  void resolve_temporal();
  void send_temporal_inputs(const RenderTexture2D& before);
  void end_temporal();
  // What post-processing starts from: the mixed picture, or the lit one.
  const RenderTexture2D& lit_picture() const;

  // --- Sun shadows (renderer_shadows.cpp) -------------------------------------------------------

  // Renders the scene's depth from the sun and hands the result to the lit shader.
  void draw_shadow_map(const Camera3D& camera, const Environment& env);
  void draw_sun_depth(const Camera3D& camera, Vec3 to_sun, const Environment& env);
  // Each cascade into its tile of the map, its matrix into light_matrices.
  void draw_cascades(const render_math::Cascades& cascades, Vec3 viewer);
  // Draws the depth from the sun's point of view into the viewport set. Returns the matrix that
  // leads into that view.
  Matrix draw_depth_from(const Camera3D& sun_view, Vec3 viewer);
  void send_cascades(const render_math::Cascades& cascades, float sun_size);

  // --- Reflections (renderer_reflections.cpp) ---------------------------------------------------

  // Captures one more face of the camera's surroundings into the panorama the lit shader reflects.
  // Six frames make a full turn, so reflections follow the camera with a short delay.
  void capture_reflection_face(const Camera3D& camera);
  void draw_surroundings(const render_math::CubeFace& face, Vector3 eye);
  void copy_face_to_panorama(const render_math::CubeFace& face);
  void blur_panorama_levels();

  // --- Post-processing (renderer_post.cpp) ------------------------------------------------------

  // hdr -> the frames before mixed in -> bloom -> tone mapping -> anti-aliasing -> `output` (the
  // screen when null).
  void post_process(const Environment& env, const RenderTexture2D* output);
  void draw_occlusion();
  void draw_bloom();
  // Blurs `picture` across, then down. `scratch` (the same size) holds the half-way result.
  void blur(const RenderTexture2D& picture, const RenderTexture2D& scratch);
  void tone_map(const Environment& env);
  void send_grade(const Environment& env);
  void antialias(const RenderTexture2D* output);
  // The last pass's shader, its values set: smoothing, or after the frames are mixed, sharpening.
  const Shader& begin_finish(int width, int height);
  void copy_depth_to(const RenderTexture2D& output);

  // --- Over the picture (renderer_overlay.cpp) --------------------------------------------------

  void draw_hud_text(const HudItem& item);
  void draw_hud_image(const HudItem& item);
};

}  // namespace redgem
