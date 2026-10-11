#include "render/renderer.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/ui/embedded_fonts.hpp"
#include "render/culling.hpp"
#include "render/environment.hpp"
#include "render/gpu_select.hpp"
#include "render/mesh_builder.hpp"
#include "render/shaders.hpp"

namespace redgem {
namespace {

constexpr const char* kDesktopShaderVersion = "#version 330";
#if defined(GRAPHICS_API_OPENGL_ES3)
constexpr const char* kShaderHeader =
    "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;";
#else
constexpr const char* kShaderHeader = kDesktopShaderVersion;
#endif
// On-screen touch controls: white, see-through, more solid under a finger.
constexpr unsigned char kTouchShade = 255;
constexpr unsigned char kTouchBaseAlpha = 28;
constexpr unsigned char kTouchRestingAlpha = 70;
constexpr unsigned char kTouchPressedAlpha = 150;
constexpr float kHalfFactor = 0.5f;
constexpr int kQuadCorners = 4;
constexpr float kParticleGlowBoost = 4.0f;  // how much brighter than its color a glowing particle is
// Sky dome (around the camera; its size only has to fit between the near and far planes).
constexpr float kSkyRadius = 50.0f;
constexpr int kSkyRings = 16;
constexpr int kSkySlices = 32;
// Bloom: what is brighter than the threshold (in linear light) glows; blurred at a fraction of
// the resolution, which is both cheaper and wider.
constexpr int kBloomDownscale = 4;
constexpr float kBloomThreshold = 1.0f;
constexpr int kBloomBlurPasses = 3;
constexpr float kBlendAsCutoutThreshold = 0.35f;  // alpha below this is cut away on "blend" textures
constexpr int kDepthBufferBit = 0x00000100;       // GL_DEPTH_BUFFER_BIT
// Sun shadows: one map covering a square of the world around where the camera looks.
constexpr int kShadowMapSize = 4096;
constexpr float kShadowExtent = 90.0f;       // meters covered by the map (side of the square)
constexpr float kShadowFocusAhead = 0.3f;    // the square is centered this fraction of its side ahead of the camera
constexpr float kShadowSunDistance = 300.0f; // how far back the sun's viewpoint sits (casters behind the camera count)
constexpr int kShadowTextureSlot = 10;       // a material map slot the engine does not use
constexpr int kDepthFormat24 = 19;           // raylib's internal id of a 24-bit depth texture
constexpr float kMinSunHeight = 0.02f;       // below this the sun is treated as set: no shadows
// Parts that look smaller than this (radius over distance from the camera) are skipped: a car's
// bolts at 50 m. Shadows of small things disappear sooner.
constexpr float kMinVisibleAngle = 0.0025f;
constexpr float kMinShadowAngle = 0.008f;
constexpr float kLightFadeDistance = 12.0f;   // meters over which the farthest used lights fade out
constexpr float kSkinnedBoundsMargin = 1.5f;  // animated limbs reach beyond the bind pose
// Reflections: the surroundings of the camera, captured one cube face per frame into a panorama.
constexpr int kReflectionFaceSize = 256;
constexpr int kPanoramaWidth = 1024;
constexpr int kPanoramaHeight = 512;
constexpr int kCubeFaces = 6;
constexpr float kRightAngle = 90.0f;
constexpr float kMinReflectedAngle = 0.03f;  // only sizeable things are worth reflecting
constexpr float kReflectionEyeHeight = 1.2f; // lowest world height (y) the capture is taken from
constexpr int kReflectionTextureSlot = 9;    // a material map slot the engine does not use
struct CubeFace {
  Vector3 forward;
  Vector3 up;
};
constexpr CubeFace kCubeFaceViews[kCubeFaces] = {
    {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},  {{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},  {{0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},  {{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
};
constexpr int kOcclusionDownscale = 2;  // ambient occlusion is computed at half resolution
// The sun's view only needs the depth range that can hold casters and receivers; a tight range
// keeps depth precise (and the shadow bias small).
constexpr double kShadowNearPlane = 1.0;
constexpr double kShadowFarPlane = 2.0 * kShadowSunDistance;
constexpr float kNearlyVertical = 0.999f;    // |y| of a sun direction beyond which Y cannot be its "up"
constexpr float kOn = 1.0f;                  // shader switches passed as floats
constexpr float kOff = 0.0f;
constexpr Vector3 kUpAxis{0.0f, 1.0f, 0.0f};
constexpr Vector3 kForwardAxis{0.0f, 0.0f, 1.0f};
constexpr ::Color kWhite{255, 255, 255, 255};  // also the tint that leaves a drawn texture unchanged
constexpr ::Color kBlack{0, 0, 0, 255};

::Color to_raylib(redgem::Color c) {
  auto channel = [](float v) {
    return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * config::kMaxColorByte);
  };
  return {channel(c.r), channel(c.g), channel(c.b), channel(c.a)};
}

Vector3 to_raylib(Vec3 v) { return {v.x, v.y, v.z}; }

// A glTF matrix (16 floats, column by column) as a raylib Matrix (whose fields are declared row by row).
Matrix from_column_major(const std::array<float, 16>& m) {
  return {m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]};
}

Matrix model_matrix(const Transform& t) {
  const Matrix scale = MatrixScale(t.scale.x, t.scale.y, t.scale.z);
  const Matrix rotation = QuaternionToMatrix({t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w});
  const Matrix translation = MatrixTranslate(t.position.x, t.position.y, t.position.z);
  return MatrixMultiply(MatrixMultiply(scale, rotation), translation);
}

constexpr int kTraceLineLength = 512;
constexpr const char* kRendererLine = "Renderer:";

// Passes on raylib's warnings and the one line that says which graphics card draws the game.
void report_graphics_card(int level, const char* format, va_list args) {
  char line[kTraceLineLength];
  std::vsnprintf(line, sizeof(line), format, args);
  const std::string text = line;
  if (text.find(kRendererLine) != std::string::npos) {
    log::info("graphics card: " + text.substr(text.find(kRendererLine) + std::string(kRendererLine).size()));
  } else if (level >= LOG_WARNING) {
    log::warning(text);
  }
}

struct GpuPrimitive {
  Mesh mesh{};
  int material = kNoIndex;
};

}  // namespace

// Texts drawn by games (UI.text): rasterized at this height and scaled to the size asked for.
constexpr int kUiFontPixels = 72;
constexpr int kUiFirstLetter = 32;    // space
constexpr int kUiLastLetter = 255;    // through Latin-1: accented letters
constexpr float kUiLetterSpacing = 0.0f;  // extra space between letters, as a share of the text size
constexpr int kUiCornerSegments = 8;
constexpr float kDiameterPerRadius = 2.0f;
// raylib's vertex buffer slots for a mesh's positions and normals.
constexpr int kPositionBuffer = 0;
constexpr int kNormalBuffer = 2;
constexpr int kNoLocation = -1;       // a uniform the shader does not have
constexpr int kTriangleCorners = 3;
constexpr float kMaxColorByte = 255.0f;
// The texture maps materials use (base color, emission, normal, metallic-roughness, occlusion).
constexpr int kMaterialMaps[] = {MATERIAL_MAP_ALBEDO, MATERIAL_MAP_EMISSION, MATERIAL_MAP_NORMAL, MATERIAL_MAP_ROUGHNESS,
                                 MATERIAL_MAP_OCCLUSION};
constexpr int kNoMaterialYet = -2;  // no material index (kNoIndex is the fallback material)

struct Renderer::Impl {
  RenderSettings settings;
  Shader shader{};
  Shader depth_shader{};
  Shader sky_shader{};
  Shader particle_shader{};
  int particle_boost_loc = -1;
  int particle_textured_loc = -1;
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
  RenderTexture2D hdr{};      // the lit scene, linear HDR, with depth
  float motion_blur = 0.0f;       // 0..1, asked for by the game (Camera.motion_blur)
  Matrix pass_view_projection{};  // of the pass being drawn (see begin_pass)
  std::vector<Transform> worlds;  // every object's world transform this frame (see place_objects)
  Font ui_font{};
  bool owns_ui_font = false;
  std::filesystem::path images_dir;
  std::unordered_map<std::string, Texture2D> ui_images;  // by name; id 0 = looked for and not found
  RenderTexture2D bloom_a{};  // bright parts, blurred (ping-pong with bloom_b)
  RenderTexture2D bloom_b{};
  RenderTexture2D ldr{};      // tone mapped, luma in alpha, before anti-aliasing
  RenderTexture2D shadow{};   // depth of the scene seen from the sun (no color)
  float wind = 0.0f;          // of the environment being drawn
  Vector3 sun_travel{};       // direction the sun's light travels this frame

  // The shaders are written for desktop OpenGL 3.3. Phones speak OpenGL ES 3: the same language
  // with another first line, and no say on how precise numbers are unless told. Either way the
  // shader learns here how many bones a skeleton may have.
  static std::string for_this_gpu(const char* source) {
    const std::string text = source;
    const std::size_t version = text.find(kDesktopShaderVersion);
    if (version == std::string::npos) return text;
    const std::string limits = "\n#define MAX_BONES " + std::to_string(config::kMaxSkinJoints);
    // (ES wants the version on the very first line: what comes before it goes.)
    return std::string(kShaderHeader) + limits + text.substr(version + std::string(kDesktopShaderVersion).size());
  }

  static Shader compile(const char* vertex, const char* fragment, const char* name) {
    const std::string vertex_text = vertex != nullptr ? for_this_gpu(vertex) : std::string();
    const std::string fragment_text = fragment != nullptr ? for_this_gpu(fragment) : std::string();
    const Shader compiled = LoadShaderFromMemory(vertex != nullptr ? vertex_text.c_str() : nullptr,
                                                 fragment != nullptr ? fragment_text.c_str() : nullptr);
    if (!IsShaderValid(compiled)) throw std::runtime_error(std::string("failed to compile the ") + name + " shader");
    return compiled;
  }

  void load_shader() {
    shader = compile(shaders::kLitVertex, shaders::kLitFragment, "lit");
    depth_shader = compile(shaders::kLitVertex, shaders::kDepthFragment, "shadow depth");
    depth_shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(depth_shader, shaders::kModelMatrix);
    depth_alpha_cutoff_loc = GetShaderLocation(depth_shader, shaders::kAlphaCutoff);
    depth_wind_loc = GetShaderLocation(depth_shader, shaders::kWindAmount);
    depth_time_loc = GetShaderLocation(depth_shader, shaders::kTime);
    sky_shader = compile(shaders::kSkyVertex, shaders::kSkyFragment, "sky");
    particle_shader = compile(nullptr, shaders::kParticleFragment, "particles");
    particle_boost_loc = GetShaderLocation(particle_shader, shaders::kParticleBoost);
    particle_textured_loc = GetShaderLocation(particle_shader, shaders::kParticleTextured);
    bright_shader = compile(nullptr, shaders::kBrightFragment, "bloom threshold");
    blur_shader = compile(nullptr, shaders::kBlurFragment, "blur");
    composite_shader = compile(nullptr, shaders::kCompositeFragment, "tone mapping");
    antialias_shader = compile(nullptr, shaders::kAntialiasFragment, "anti-aliasing");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, shaders::kModelMatrix);
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, shaders::kNormalMatrix);
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, shaders::kViewPosition);
    shader.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(shader, shaders::kEmissiveMap);
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, shaders::kNormalMap);
    shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(shader, shaders::kSurfaceMap);
    shader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(shader, shaders::kOcclusionMap);
    has_normal_map_loc = GetShaderLocation(shader, shaders::kHasNormalMap);
    normal_scale_loc = GetShaderLocation(shader, shaders::kNormalScale);
    has_surface_map_loc = GetShaderLocation(shader, shaders::kHasSurfaceMap);
    occlusion_strength_loc = GetShaderLocation(shader, shaders::kOcclusionStrength);
    shadow_map_loc = GetShaderLocation(shader, shaders::kShadowMap);
    light_matrix_loc = GetShaderLocation(shader, shaders::kLightViewProjection);
    shadow_strength_loc = GetShaderLocation(shader, shaders::kShadowStrength);
    shadow_texel_loc = GetShaderLocation(shader, shaders::kShadowTexel);
    shadow = load_shadow_target(kShadowMapSize);
    panorama_shader = compile(nullptr, shaders::kPanoramaFragment, "reflection panorama");
    occlusion_shader = compile(nullptr, shaders::kAmbientOcclusionFragment, "ambient occlusion");
    reflection_map_loc = GetShaderLocation(shader, shaders::kReflectionMap);
    reflection_strength_loc = GetShaderLocation(shader, shaders::kReflectionStrength);
    reflection_face = load_target(kReflectionFaceSize, kReflectionFaceSize, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, true);
    panorama = load_target(kPanoramaWidth, kPanoramaHeight, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, false);
    // Around the horizon the panorama wraps; at the poles it must not (up would blend with down).
    rlTextureParameters(panorama.texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
    rlTextureParameters(panorama.texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
    sun_direction_loc = GetShaderLocation(shader, shaders::kSunDirection);
    sun_color_loc = GetShaderLocation(shader, shaders::kSunColor);
    ambient_loc = GetShaderLocation(shader, shaders::kAmbientStrength);
    point_count_loc = GetShaderLocation(shader, shaders::kPointLightCount);
    point_positions_loc = GetShaderLocation(shader, shaders::kPointLightPositions);
    point_colors_loc = GetShaderLocation(shader, shaders::kPointLightColors);
    point_ranges_loc = GetShaderLocation(shader, shaders::kPointLightRanges);
    fog_color_loc = GetShaderLocation(shader, shaders::kFogColor);
    fog_density_loc = GetShaderLocation(shader, shaders::kFogDensity);
    metallic_loc = GetShaderLocation(shader, shaders::kMetallic);
    roughness_loc = GetShaderLocation(shader, shaders::kRoughness);
    emissive_loc = GetShaderLocation(shader, shaders::kEmissive);
    has_emissive_map_loc = GetShaderLocation(shader, shaders::kHasEmissiveMap);
    alpha_cutoff_loc = GetShaderLocation(shader, shaders::kAlphaCutoff);
    tint_loc = GetShaderLocation(shader, shaders::kTint);
    skinned_loc = GetShaderLocation(shader, shaders::kSkinned);
    depth_skinned_loc = GetShaderLocation(depth_shader, shaders::kSkinned);
    shader.locs[SHADER_LOC_BONE_MATRICES] = GetShaderLocation(shader, shaders::kBoneMatrices);
    depth_shader.locs[SHADER_LOC_BONE_MATRICES] = GetShaderLocation(depth_shader, shaders::kBoneMatrices);
    wind_loc = GetShaderLocation(shader, shaders::kWindAmount);
    time_loc = GetShaderLocation(shader, shaders::kTime);
    sky_top_loc = GetShaderLocation(shader, shaders::kSkyTop);
    sky_horizon_loc = GetShaderLocation(shader, shaders::kSkyHorizon);
    ground_loc = GetShaderLocation(shader, shaders::kGroundColor);
    fallback_material = make_material(redgem::Color{});
    sky_mesh = GenMeshSphere(kSkyRadius, kSkyRings, kSkySlices);
    sky_material = LoadMaterialDefault();
    sky_material.shader = sky_shader;
  }

  Material make_material(redgem::Color base_color) {
    Material material = LoadMaterialDefault();
    material.shader = shader;
    material.maps[MATERIAL_MAP_ALBEDO].color = to_raylib(base_color);
    return material;
  }

  void release() {
    release_assets();
    release_targets();
    if (fallback_material.maps != nullptr) MemFree(fallback_material.maps);
    fallback_material = Material{};
    if (sky_material.maps != nullptr) MemFree(sky_material.maps);
    sky_material = Material{};
    if (sky_mesh.vaoId != 0) UnloadMesh(sky_mesh);
    sky_mesh = Mesh{};
    release_ui_images();
    if (owns_ui_font) UnloadFont(ui_font);
    owns_ui_font = false;
    if (shadow.id != 0) {
      rlUnloadTexture(shadow.depth.id);
      rlUnloadFramebuffer(shadow.id);
      shadow = RenderTexture2D{};
    }
    for (RenderTexture2D* target : {&reflection_face, &panorama}) {
      if (target->id != 0) UnloadRenderTexture(*target);
      *target = RenderTexture2D{};
    }
    for (Shader* loaded : {&shader, &depth_shader, &panorama_shader, &occlusion_shader, &sky_shader, &particle_shader, &bright_shader, &blur_shader, &composite_shader, &antialias_shader}) {
      if (loaded->id != 0) UnloadShader(*loaded);
      *loaded = Shader{};
    }
  }

  void release_assets() {
    for (auto& primitives : meshes) {
      for (GpuPrimitive& primitive : primitives) UnloadMesh(primitive.mesh);
    }
    meshes.clear();
    mesh_bounds.clear();
    skins.clear();
    for (Texture2D& texture : textures) UnloadTexture(texture);
    textures.clear();
    // Materials share the shader and textures released above; only free their map arrays.
    for (Material& material : materials) MemFree(material.maps);
    materials.clear();
    material_factors.clear();
  }

  // --- Post-processing targets ----------------------------------------------------------------

  static RenderTexture2D load_target(int width, int height, int format, bool with_depth) {
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    rlEnableFramebuffer(target.id);
    target.texture.id = rlLoadTexture(nullptr, width, height, format, 1);
    target.texture.width = width;
    target.texture.height = height;
    target.texture.format = format;
    target.texture.mipmaps = 1;
    rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    if (with_depth) {
      // A texture (not a render buffer): the ambient occlusion pass reads it.
      target.depth.id = rlLoadTextureDepth(width, height, false);
      target.depth.width = width;
      target.depth.height = height;
      target.depth.format = kDepthFormat24;
      target.depth.mipmaps = 1;
      rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    }
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
  static RenderTexture2D load_shadow_target(int size) {
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    target.texture.width = size;
    target.texture.height = size;
    rlEnableFramebuffer(target.id);
    target.depth.id = rlLoadTextureDepth(size, size, false);
    target.depth.width = size;
    target.depth.height = size;
    target.depth.format = kDepthFormat24;
    target.depth.mipmaps = 1;
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    const bool complete = rlFramebufferComplete(target.id);
    rlDisableFramebuffer();
    if (!complete) throw std::runtime_error("cannot create the shadow map");
    return target;
  }

  void release_targets() {
    for (RenderTexture2D* target : {&hdr, &bloom_a, &bloom_b, &ldr, &occlusion_a, &occlusion_b}) {
      if (target->id != 0) UnloadRenderTexture(*target);
      *target = RenderTexture2D{};
    }
  }

  void ensure_targets(int width, int height) {
    if (hdr.id != 0 && hdr.texture.width == width && hdr.texture.height == height) return;
    release_targets();
    const int bloom_width = std::max(width / kBloomDownscale, 1);
    const int bloom_height = std::max(height / kBloomDownscale, 1);
    hdr = load_target(width, height, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, true);
    bloom_a = load_target(bloom_width, bloom_height, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, false);
    bloom_b = load_target(bloom_width, bloom_height, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, false);
    ldr = load_target(width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, false);
    const int occlusion_width = std::max(width / kOcclusionDownscale, 1);
    const int occlusion_height = std::max(height / kOcclusionDownscale, 1);
    occlusion_a = load_target(occlusion_width, occlusion_height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, false);
    occlusion_b = load_target(occlusion_width, occlusion_height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, false);
  }

  // Draws `source` over the whole current target (render textures are stored upside down).
  static void draw_full(const RenderTexture2D& source, int width, int height) {
    const Rectangle from{0.0f, 0.0f, static_cast<float>(source.texture.width), -static_cast<float>(source.texture.height)};
    const Rectangle to{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    DrawTexturePro(source.texture, from, to, Vector2{0.0f, 0.0f}, 0.0f, kWhite);
  }

  void pass(const RenderTexture2D& target, Shader& effect, const RenderTexture2D& source) {
    BeginTextureMode(target);
    BeginShaderMode(effect);
    draw_full(source, target.texture.width, target.texture.height);
    EndShaderMode();
    EndTextureMode();
  }

  // hdr -> bloom -> tone mapping -> anti-aliasing -> `output` (the screen when null).
  void post_process(const Environment& env, const RenderTexture2D* output) {
    const int width = hdr.texture.width;
    const int height = hdr.texture.height;
    rlDrawRenderBatchActive();
    rlDisableColorBlend();  // these passes replace pixels (alpha carries data, not transparency)

    // Ambient occlusion from the depth the scene left behind, then a blur to hide its sampling noise.
    if (env.ambient_occlusion > 0.0f) {
      RenderTexture2D depth_view{};
      depth_view.texture = hdr.depth;
      const Vector2 occlusion_size{static_cast<float>(occlusion_a.texture.width), static_cast<float>(occlusion_a.texture.height)};
      const float near_plane = static_cast<float>(rlGetCullDistanceNear());
      const float far_plane = static_cast<float>(rlGetCullDistanceFar());
      SetShaderValue(occlusion_shader, GetShaderLocation(occlusion_shader, shaders::kTargetSize), &occlusion_size, SHADER_UNIFORM_VEC2);
      SetShaderValue(occlusion_shader, GetShaderLocation(occlusion_shader, shaders::kTanHalfFov), &view_tan_half_fov, SHADER_UNIFORM_FLOAT);
      SetShaderValue(occlusion_shader, GetShaderLocation(occlusion_shader, shaders::kAspect), &view_aspect, SHADER_UNIFORM_FLOAT);
      SetShaderValue(occlusion_shader, GetShaderLocation(occlusion_shader, shaders::kNearPlane), &near_plane, SHADER_UNIFORM_FLOAT);
      SetShaderValue(occlusion_shader, GetShaderLocation(occlusion_shader, shaders::kFarPlane), &far_plane, SHADER_UNIFORM_FLOAT);
      pass(occlusion_a, occlusion_shader, depth_view);
      const int blur_loc = GetShaderLocation(blur_shader, shaders::kBlurStep);
      const Vector2 across{1.0f / occlusion_size.x, 0.0f};
      SetShaderValue(blur_shader, blur_loc, &across, SHADER_UNIFORM_VEC2);
      pass(occlusion_b, blur_shader, occlusion_a);
      const Vector2 down{0.0f, 1.0f / occlusion_size.y};
      SetShaderValue(blur_shader, blur_loc, &down, SHADER_UNIFORM_VEC2);
      pass(occlusion_a, blur_shader, occlusion_b);
    }

    const float threshold = kBloomThreshold;
    SetShaderValue(bright_shader, GetShaderLocation(bright_shader, shaders::kThreshold), &threshold, SHADER_UNIFORM_FLOAT);
    pass(bloom_a, bright_shader, hdr);
    const int step_loc = GetShaderLocation(blur_shader, shaders::kBlurStep);
    for (int i = 0; i < kBloomBlurPasses; ++i) {
      const Vector2 horizontal{1.0f / static_cast<float>(bloom_a.texture.width), 0.0f};
      SetShaderValue(blur_shader, step_loc, &horizontal, SHADER_UNIFORM_VEC2);
      pass(bloom_b, blur_shader, bloom_a);
      const Vector2 vertical{0.0f, 1.0f / static_cast<float>(bloom_a.texture.height)};
      SetShaderValue(blur_shader, step_loc, &vertical, SHADER_UNIFORM_VEC2);
      pass(bloom_a, blur_shader, bloom_b);
    }

    BeginTextureMode(ldr);
    BeginShaderMode(composite_shader);
    SetShaderValue(composite_shader, GetShaderLocation(composite_shader, shaders::kBloomStrength), &env.bloom, SHADER_UNIFORM_FLOAT);
    SetShaderValue(composite_shader, GetShaderLocation(composite_shader, shaders::kExposure), &env.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(composite_shader, GetShaderLocation(composite_shader, shaders::kVignette), &env.vignette, SHADER_UNIFORM_FLOAT);
    const float grade_time = static_cast<float>(GetTime());
    const std::pair<const char*, const float*> grade[] = {
        {shaders::kContrast, &env.contrast}, {shaders::kSaturation, &env.saturation}, {shaders::kGrain, &env.grain},
        {shaders::kAberration, &env.aberration}, {shaders::kMotionBlur, &motion_blur}, {shaders::kGradeTime, &grade_time}};
    for (const auto& [name, value] : grade) {
      SetShaderValue(composite_shader, GetShaderLocation(composite_shader, name), value, SHADER_UNIFORM_FLOAT);
    }
    SetShaderValueTexture(composite_shader, GetShaderLocation(composite_shader, shaders::kBloomTexture), bloom_a.texture);
    SetShaderValue(composite_shader, GetShaderLocation(composite_shader, shaders::kOcclusionAmount), &env.ambient_occlusion, SHADER_UNIFORM_FLOAT);
    SetShaderValueTexture(composite_shader, GetShaderLocation(composite_shader, shaders::kOcclusionTexture), occlusion_a.texture);
    draw_full(hdr, width, height);
    EndShaderMode();
    EndTextureMode();

    if (output != nullptr) BeginTextureMode(*output);
    BeginShaderMode(antialias_shader);
    const Vector2 texel{1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height)};
    SetShaderValue(antialias_shader, GetShaderLocation(antialias_shader, shaders::kTexelSize), &texel, SHADER_UNIFORM_VEC2);
    draw_full(ldr, width, height);
    EndShaderMode();
    if (output != nullptr) {
      EndTextureMode();
      // The scene's depth goes along, so overlays drawn into `output` afterwards are occluded by it.
      rlBindFramebuffer(RL_READ_FRAMEBUFFER, hdr.id);
      rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, output->id);
      rlBlitFramebuffer(0, 0, width, height, 0, 0, width, height, kDepthBufferBit);
      rlDisableFramebuffer();
    } else {
      rlDrawRenderBatchActive();
    }
    rlEnableColorBlend();
  }

  // --- Drawing ----------------------------------------------------------------------------------

  // Sets a material's uniforms (only cut-out and wind for the shadow depth pass). Returns true
  // when it must be drawn without backface culling.
  bool apply_material(int index, bool depth_pass) {
    MaterialDesc factors;  // the fallback: white dielectric
    if (index != kNoIndex && static_cast<std::size_t>(index) < material_factors.size()) factors = material_factors[index];
    if (depth_pass) {
      const bool has_texture = factors.base_color_image != kNoIndex;
      const bool cut = factors.alpha_mode == AlphaMode::Mask || (factors.alpha_mode == AlphaMode::Blend && has_texture);
      const float threshold = factors.alpha_mode == AlphaMode::Mask ? factors.alpha_cutoff : cut ? kBlendAsCutoutThreshold : 0.0f;
      const float moving = cut && has_texture ? wind : 0.0f;
      SetShaderValue(depth_shader, depth_alpha_cutoff_loc, &threshold, SHADER_UNIFORM_FLOAT);
      SetShaderValue(depth_shader, depth_wind_loc, &moving, SHADER_UNIFORM_FLOAT);
      return factors.double_sided || cut;
    }
    const float has_normal_map = factors.normal_image != kNoIndex ? 1.0f : 0.0f;
    const float has_surface_map = factors.metallic_roughness_image != kNoIndex ? 1.0f : 0.0f;
    const float occlusion = factors.occlusion_image != kNoIndex ? factors.occlusion_strength : 0.0f;
    SetShaderValue(shader, has_normal_map_loc, &has_normal_map, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, normal_scale_loc, &factors.normal_scale, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, has_surface_map_loc, &has_surface_map, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, occlusion_strength_loc, &occlusion, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, metallic_loc, &factors.metallic, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, roughness_loc, &factors.roughness, SHADER_UNIFORM_FLOAT);
    const Vector3 emissive = to_raylib(factors.emissive);
    SetShaderValue(shader, emissive_loc, &emissive, SHADER_UNIFORM_VEC3);
    const float has_emissive_map = factors.emissive_image != kNoIndex ? 1.0f : 0.0f;
    SetShaderValue(shader, has_emissive_map_loc, &has_emissive_map, SHADER_UNIFORM_FLOAT);
    // There is no sorted transparency: cut-out materials (and textured "blend" ones, i.e. leaves
    // and fences) are drawn with an alpha test instead.
    const bool textured = factors.base_color_image != kNoIndex;
    const bool cutout = factors.alpha_mode == AlphaMode::Mask || (factors.alpha_mode == AlphaMode::Blend && textured);
    const float cutoff = factors.alpha_mode == AlphaMode::Mask ? factors.alpha_cutoff : cutout ? kBlendAsCutoutThreshold : 0.0f;
    SetShaderValue(shader, alpha_cutoff_loc, &cutoff, SHADER_UNIFORM_FLOAT);
    const float sway = cutout && textured ? wind : 0.0f;  // foliage sways in the wind
    SetShaderValue(shader, wind_loc, &sway, SHADER_UNIFORM_FLOAT);
    return factors.double_sided || cutout;
  }

  Camera3D camera_for(const Scene& scene) const {
    Camera3D camera{};
    camera.projection = CAMERA_PERSPECTIVE;
    const ObjectId id = scene.main_camera();
    if (id == kNoObject) {
      camera.position = {0.0f, config::kDefaultCameraDistance * kHalfFactor, config::kDefaultCameraDistance};
      camera.target = {0.0f, 0.0f, 0.0f};
      camera.up = {0.0f, 1.0f, 0.0f};
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

  // Sends the sun, the point lights nearest to the viewer and the environment to the shaders.
  void apply_lights(const Scene& scene, const Environment& env, Vec3 viewer) {
    Vec3 sun_direction = normalize(
        Vec3{config::kDefaultSunDirectionX, config::kDefaultSunDirectionY, config::kDefaultSunDirectionZ});
    Vec3 sun_color = Vec3::one() * config::kDefaultSunIntensity;
    std::vector<PointLight> candidates;
    bool sun_found = false;
    for (const ObjectId id : scene.alive_ids()) {
      const GameObject& object = scene.at(id);
      if (!object.light || object.prefab) continue;
      const LightDesc& light = *object.light;
      const Transform world = scene.world_transform(id);
      const Vec3 color{light.color.r, light.color.g, light.color.b};
      if (light.type == LightType::Directional) {
        if (sun_found) continue;
        sun_found = true;
        sun_direction = world.forward();
        sun_color = color * (light.intensity * config::kSunStrengthScale);
      } else {
        candidates.push_back({world.position, color * (light.intensity * config::kPointWattScale),
                              light.range > 0.0f ? light.range : config::kDefaultPointLightRange});
      }
    }
    const std::vector<PointLight> lights =
        nearest_lights(std::move(candidates), viewer, shaders::kMaxPointLights, kLightFadeDistance);
    Vector3 positions[shaders::kMaxPointLights] = {};
    Vector3 colors[shaders::kMaxPointLights] = {};
    float ranges[shaders::kMaxPointLights] = {};
    const int point_count = static_cast<int>(lights.size());
    for (int i = 0; i < point_count; ++i) {
      positions[i] = to_raylib(lights[i].position);
      colors[i] = to_raylib(lights[i].color);
      ranges[i] = lights[i].range;
    }
    const Vector3 sun_dir = to_raylib(sun_direction);
    const Vector3 sun_col = to_raylib(sun_color);
    const Vector3 sky_top = to_raylib(env.sky_top);
    const Vector3 sky_horizon = to_raylib(env.sky_horizon);
    const Vector3 ground = to_raylib(env.ground);
    const Vector3 fog = to_raylib(env.fog_color);
    const float time = static_cast<float>(GetTime());
    SetShaderValue(shader, sun_direction_loc, &sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, sun_color_loc, &sun_col, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, ambient_loc, &env.ambient, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, point_count_loc, &point_count, SHADER_UNIFORM_INT);
    if (point_count > 0) {
      SetShaderValueV(shader, point_positions_loc, positions, SHADER_UNIFORM_VEC3, point_count);
      SetShaderValueV(shader, point_colors_loc, colors, SHADER_UNIFORM_VEC3, point_count);
      SetShaderValueV(shader, point_ranges_loc, ranges, SHADER_UNIFORM_FLOAT, point_count);
    }
    SetShaderValue(shader, sky_top_loc, &sky_top, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, sky_horizon_loc, &sky_horizon, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, ground_loc, &ground, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, fog_color_loc, &fog, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, fog_density_loc, &env.fog_density, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, time_loc, &time, SHADER_UNIFORM_FLOAT);
    // The sky dome shows the same sky the surfaces are lit by.
    SetShaderValue(sky_shader, GetShaderLocation(sky_shader, shaders::kSkyTop), &sky_top, SHADER_UNIFORM_VEC3);
    SetShaderValue(sky_shader, GetShaderLocation(sky_shader, shaders::kSkyHorizon), &sky_horizon, SHADER_UNIFORM_VEC3);
    SetShaderValue(sky_shader, GetShaderLocation(sky_shader, shaders::kGroundColor), &ground, SHADER_UNIFORM_VEC3);
    SetShaderValue(sky_shader, GetShaderLocation(sky_shader, shaders::kSunDirection), &sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(sky_shader, GetShaderLocation(sky_shader, shaders::kSunColor), &sun_col, SHADER_UNIFORM_VEC3);
    SetShaderValue(sky_shader, GetShaderLocation(sky_shader, shaders::kSunGlow), &env.sun_glow, SHADER_UNIFORM_FLOAT);
    wind = env.wind;
    sun_travel = to_raylib(sun_direction);
  }

  // Where every object is in the world this frame, worked out once: each of the frame's passes
  // (shadows, reflections, the view) would otherwise walk every object's chain of parents again.
  void place_objects(const Scene& scene) {
    const std::size_t count = scene.size();
    worlds.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
      const GameObject& object = scene.at(static_cast<ObjectId>(i));
      if (object.destroyed) continue;
      // Parents are created before their children, so theirs is already there (when it is not,
      // e.g. after a reparent, ask the scene).
      const bool parent_placed = object.parent != kNoObject && static_cast<std::size_t>(object.parent) < i;
      worlds[i] = object.parent == kNoObject ? object.local
                  : parent_placed          ? combine(worlds[static_cast<std::size_t>(object.parent)], object.local)
                                           : scene.world_transform(object.id);
    }
  }

  // The font of the game's texts: the interface font, rasterized large and scaled down smoothly.
  void load_ui_font() {
    const fonts::EmbeddedFont inter = fonts::interface_font();
    std::vector<int> letters;
    for (int code = kUiFirstLetter; code <= kUiLastLetter; ++code) letters.push_back(code);
    ui_font = LoadFontFromMemory(".ttf", inter.data, inter.size, kUiFontPixels, letters.data(), static_cast<int>(letters.size()));
    if (ui_font.texture.id == 0) {
      ui_font = GetFontDefault();
      return;
    }
    GenTextureMipmaps(&ui_font.texture);
    SetTextureFilter(ui_font.texture, TEXTURE_FILTER_TRILINEAR);
    owns_ui_font = true;
  }

  // The picture UI.image or a particle asks for by name, loaded on first use. Null (reported once) when the
  // game's images folder has no such file.
  const Texture2D* ui_image(const std::string& name) {
    const auto found = ui_images.find(name);
    if (found != ui_images.end()) return found->second.id != 0 ? &found->second : nullptr;
    Texture2D picture{};
    for (const char* extension : {"", ".png", ".jpg", ".jpeg"}) {
      const std::filesystem::path file = images_dir / (name + extension);
      std::error_code fs_error;
      if (!std::filesystem::is_regular_file(file, fs_error)) continue;
      picture = LoadTexture(file.string().c_str());
      break;
    }
    if (picture.id == 0) {
      log::error("no picture named '" + name + "' in " + images_dir.string());
    } else {
      SetTextureFilter(picture, TEXTURE_FILTER_BILINEAR);
    }
    return ui_images.emplace(name, picture).first->second.id != 0 ? &ui_images.at(name) : nullptr;
  }

  void release_ui_images() {
    for (auto& [name, picture] : ui_images) {
      if (picture.id != 0) UnloadTexture(picture);
    }
    ui_images.clear();
  }

  // Fills bone_matrices for an object's skeleton: where each of its joints is now, times the
  // skin's inverse bind matrix. False when the object has no (usable) skeleton.
  bool pose_skeleton(const Scene& scene, const GameObject& object) {
    if (object.skin < 0 || static_cast<std::size_t>(object.skin) >= skins.size()) return false;
    const SkinDesc& skin = skins[static_cast<std::size_t>(object.skin)];
    const std::size_t count =
        std::min({object.joints.size(), skin.inverse_bind.size(), static_cast<std::size_t>(config::kMaxSkinJoints)});
    bone_matrices.resize(count);
    for (std::size_t j = 0; j < count; ++j) {
      if (!scene.is_alive(object.joints[j])) return false;
      bone_matrices[j] = MatrixMultiply(from_column_major(skin.inverse_bind[j]),
                                        model_matrix(worlds[static_cast<std::size_t>(object.joints[j])]));
    }
    return count > 0;
  }

  // --- Drawing meshes -----------------------------------------------------------------------
  // The engine's own draw call. raylib's DrawMesh sets the whole pipeline up again for every
  // mesh (every matrix, every texture slot, bound and unbound); a scene is thousands of meshes
  // per pass, so here what belongs to the pass is set once, what belongs to a material only
  // when the material changes, and a mesh costs its matrices and one draw.

  void begin_pass(const Shader& pass_shader) {
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

  void bind_material(const Material& material, const Shader& pass_shader) {
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
    if (color_location != kNoLocation) {
      const ::Color base = material.maps[MATERIAL_MAP_DIFFUSE].color;
      const float color[] = {static_cast<float>(base.r) / kMaxColorByte, static_cast<float>(base.g) / kMaxColorByte,
                             static_cast<float>(base.b) / kMaxColorByte, static_cast<float>(base.a) / kMaxColorByte};
      rlSetUniform(color_location, color, SHADER_UNIFORM_VEC4, 1);
    }
  }

  // `bones` is null for meshes that are not deformed by a skeleton.
  void draw_mesh(const Mesh& mesh, const Matrix& model, const Shader& pass_shader, const std::vector<Matrix>* bones) {
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
      rlDrawVertexArrayElements(0, mesh.triangleCount * kTriangleCorners, nullptr);
    } else {
      rlDrawVertexArray(0, mesh.vertexCount);
    }
  }

  // Leaves the state raylib expects: nothing bound.
  void end_pass() {
    rlDisableVertexArray();
    for (const int map : kMaterialMaps) {
      rlActiveTextureSlot(map);
      rlDisableTexture();
    }
    rlActiveTextureSlot(0);
    rlDisableShader();
  }

  // Draws what the current 3D view can see. `viewer` is the game camera in both passes: detail
  // is judged by how big things look to the player, not to the sun.
  void draw_objects(const Scene& scene, bool depth_pass, Vec3 viewer, float detail_angle = 0.0f) {
    const Frustum frustum = frustum_from_matrix(MatrixToFloatV(MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection())).v);
    const float min_angle = detail_angle > 0.0f ? detail_angle : depth_pass ? kMinShadowAngle : kMinVisibleAngle;
    const std::size_t count = std::min(scene.size(), worlds.size());
    const Shader& pass_shader = depth_pass ? depth_shader : shader;
    const int skinned_location = depth_pass ? depth_skinned_loc : skinned_loc;
    begin_pass(pass_shader);
    int last_material = kNoMaterialYet;
    bool last_two_sided = false;
    for (std::size_t slot = 0; slot < count; ++slot) {
      const GameObject& object = scene.at(static_cast<ObjectId>(slot));
      if (object.destroyed || object.mesh == kNoIndex || !object.visible || (object.prefab && !show_prefabs)) continue;
      if (static_cast<std::size_t>(object.mesh) >= meshes.size()) continue;
      const Transform& world = worlds[slot];
      const bool skinned = pose_skeleton(scene, object);
      const Bounds& bounds = mesh_bounds[static_cast<std::size_t>(object.mesh)];
      if (bounds.valid) {
        const Vec3 local_center = (bounds.min + bounds.max) * kHalfFactor;
        Vec3 center = world.position + rotate(world.rotation, hadamard(local_center, world.scale));
        float stretch = std::max({std::fabs(world.scale.x), std::fabs(world.scale.y), std::fabs(world.scale.z)});
        if (skinned) {
          // A skinned mesh is placed by its skeleton, not by its own node: follow the first bone,
          // with room for limbs that swing out of the bind pose's bounds.
          const Matrix& bone = bone_matrices.front();
          const Vector3 moved = Vector3Transform(to_raylib(local_center), bone);
          center = {moved.x, moved.y, moved.z};
          stretch = kSkinnedBoundsMargin * Vector3Length({bone.m0, bone.m1, bone.m2});  // a scaled rig is bigger
        }
        const float radius = length(bounds.max - bounds.min) * kHalfFactor * stretch;
        if (!sphere_in_frustum(frustum, center, radius) || too_small_to_see(center, radius, viewer, min_angle)) continue;
        if (outside_distance_range(center, viewer, object.visible_from, object.visible_to)) continue;
      } else if (outside_distance_range(world.position, viewer, object.visible_from, object.visible_to)) {
        continue;
      }
      const Matrix transform = model_matrix(world);

      if (!depth_pass) {
        const Vector3 tint = to_raylib(object.tint);
        rlSetUniform(tint_loc, &tint, SHADER_UNIFORM_VEC3, 1);
      }
      for (const GpuPrimitive& primitive : meshes[object.mesh]) {
        // Consecutive parts often share a material: its textures and uniforms are already set.
        if (primitive.material != last_material) {
          last_two_sided = apply_material(primitive.material, depth_pass);
          bind_material(primitive.material == kNoIndex ? fallback_material : materials[primitive.material], pass_shader);
          last_material = primitive.material;
        }
        const bool two_sided = last_two_sided;
        if (two_sided) rlDisableBackfaceCulling();
        // Per part: a skinned object may still have parts without bone weights, which stay rigid.
        const bool deformed = skinned && primitive.mesh.boneIds != nullptr;
        const float skinning = deformed ? kOn : kOff;
        rlSetUniform(skinned_location, &skinning, SHADER_UNIFORM_FLOAT, 1);
        draw_mesh(primitive.mesh, transform, pass_shader, deformed ? &bone_matrices : nullptr);
        if (two_sided) rlEnableBackfaceCulling();
      }
    }
    end_pass();
  }

  // Renders the scene's depth from the sun and hands the result to the lit shader.
  void draw_shadow_map(const Scene& scene, const Camera3D& camera, const Environment& env) {
    const Vec3 to_sun = normalize(Vec3{-sun_travel.x, -sun_travel.y, -sun_travel.z});
    float strength = to_sun.y > kMinSunHeight ? env.shadows : 0.0f;
    if (strength > 0.0f) {
      const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
      Vector3 focus = Vector3Add(camera.position, Vector3Scale(forward, kShadowExtent * kShadowFocusAhead));
      // Move the map in whole texels only: otherwise shadow edges shimmer as the camera moves.
      const Vector3 sun_axis = to_raylib(to_sun);
      // A sun straight overhead has no sideways "up": use another axis for the map's orientation.
      const Vector3 map_vertical = std::fabs(sun_axis.y) > kNearlyVertical ? kForwardAxis : kUpAxis;
      const Vector3 map_right = Vector3Normalize(Vector3CrossProduct(map_vertical, sun_axis));
      const Vector3 map_up = Vector3CrossProduct(sun_axis, map_right);
      const float texel_size = kShadowExtent / static_cast<float>(kShadowMapSize);
      const auto snapped = [texel_size](float value) { return std::floor(value / texel_size) * texel_size; };
      focus = Vector3Add(Vector3Add(Vector3Scale(map_right, snapped(Vector3DotProduct(focus, map_right))),
                                    Vector3Scale(map_up, snapped(Vector3DotProduct(focus, map_up)))),
                         Vector3Scale(sun_axis, Vector3DotProduct(focus, sun_axis)));
      Camera3D sun_view{};
      sun_view.projection = CAMERA_ORTHOGRAPHIC;
      sun_view.fovy = kShadowExtent;
      sun_view.target = focus;
      sun_view.position = Vector3Add(focus, Vector3Scale(to_raylib(to_sun), kShadowSunDistance));
      sun_view.up = map_vertical;
      const float time = static_cast<float>(GetTime());
      SetShaderValue(depth_shader, depth_time_loc, &time, SHADER_UNIFORM_FLOAT);
      const double game_near = rlGetCullDistanceNear();
      const double game_far = rlGetCullDistanceFar();
      rlSetClipPlanes(kShadowNearPlane, kShadowFarPlane);
      BeginTextureMode(shadow);
      ClearBackground(kWhite);
      BeginMode3D(sun_view);
      const Matrix light_matrix = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
      draw_objects(scene, true, Vec3{camera.position.x, camera.position.y, camera.position.z});
      EndMode3D();
      EndTextureMode();
      rlSetClipPlanes(game_near, game_far);
      SetShaderValueMatrix(shader, light_matrix_loc, light_matrix);
      const float depth_range = static_cast<float>(kShadowFarPlane - kShadowNearPlane);
      SetShaderValue(shader, GetShaderLocation(shader, shaders::kShadowDepthRange), &depth_range, SHADER_UNIFORM_FLOAT);
    }
    const float texel = 1.0f / static_cast<float>(kShadowMapSize);
    SetShaderValue(shader, shadow_strength_loc, &strength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, shadow_texel_loc, &texel, SHADER_UNIFORM_FLOAT);
    // Bound once per frame on a slot no material uses.
    rlEnableShader(shader.id);
    rlActiveTextureSlot(kShadowTextureSlot);
    rlEnableTexture(shadow.depth.id);
    const int slot = kShadowTextureSlot;
    rlSetUniform(shadow_map_loc, &slot, SHADER_UNIFORM_INT, 1);
    rlActiveTextureSlot(0);
  }

  // Captures one more face of the camera's surroundings into the panorama the lit shader reflects.
  // Six frames make a full turn, so reflections follow the camera with a short delay.
  void capture_reflection_face(const Scene& scene, const Camera3D& camera) {
    const CubeFace& face = kCubeFaceViews[next_face];
    Camera3D view{};
    view.projection = CAMERA_PERSPECTIVE;
    view.fovy = kRightAngle;
    // Not from below this height (a camera near the floor would mostly capture the floor).
    view.position = {camera.position.x, std::max(camera.position.y, kReflectionEyeHeight), camera.position.z};
    view.target = Vector3Add(view.position, face.forward);
    view.up = face.up;
    BeginTextureMode(reflection_face);
    ClearBackground(kBlack);
    BeginMode3D(view);
    draw_sky(view);
    draw_objects(scene, false, Vec3{view.position.x, view.position.y, view.position.z}, kMinReflectedAngle);
    EndMode3D();
    EndTextureMode();

    const Vector3 right = Vector3Normalize(Vector3CrossProduct(face.forward, face.up));
    const Vector3 up = Vector3CrossProduct(right, face.forward);
    const Vector2 size{static_cast<float>(kPanoramaWidth), static_cast<float>(kPanoramaHeight)};
    rlDisableColorBlend();  // each face replaces its part of the panorama
    BeginTextureMode(panorama);
    BeginShaderMode(panorama_shader);
    SetShaderValue(panorama_shader, GetShaderLocation(panorama_shader, shaders::kFaceForward), &face.forward, SHADER_UNIFORM_VEC3);
    SetShaderValue(panorama_shader, GetShaderLocation(panorama_shader, shaders::kFaceRight), &right, SHADER_UNIFORM_VEC3);
    SetShaderValue(panorama_shader, GetShaderLocation(panorama_shader, shaders::kFaceUp), &up, SHADER_UNIFORM_VEC3);
    SetShaderValue(panorama_shader, GetShaderLocation(panorama_shader, shaders::kTargetSize), &size, SHADER_UNIFORM_VEC2);
    draw_full(reflection_face, kPanoramaWidth, kPanoramaHeight);
    EndShaderMode();
    EndTextureMode();
    rlEnableColorBlend();
    // Blurrier levels for rougher surfaces.
    rlEnableTexture(panorama.texture.id);
    int mipmaps = 1;
    rlGenTextureMipmaps(panorama.texture.id, kPanoramaWidth, kPanoramaHeight, panorama.texture.format, &mipmaps);
    rlTextureParameters(panorama.texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);
    rlTextureParameters(panorama.texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
    rlDisableTexture();

    next_face = (next_face + 1) % kCubeFaces;
    faces_captured = std::min(faces_captured + 1, kCubeFaces);
    const float strength = faces_captured == kCubeFaces ? 1.0f : 0.0f;
    SetShaderValue(shader, reflection_strength_loc, &strength, SHADER_UNIFORM_FLOAT);
    rlEnableShader(shader.id);
    rlActiveTextureSlot(kReflectionTextureSlot);
    rlEnableTexture(panorama.texture.id);
    const int slot = kReflectionTextureSlot;
    rlSetUniform(reflection_map_loc, &slot, SHADER_UNIFORM_INT, 1);
    rlActiveTextureSlot(0);
  }

  // The camera's right and up: what a square has to lie along to face it.
  struct Facing {
    Vec3 right;
    Vec3 up;
  };

  static Facing facing(const Camera3D& camera) {
    const Vec3 eye{camera.position.x, camera.position.y, camera.position.z};
    const Vec3 forward = normalize(Vec3{camera.target.x, camera.target.y, camera.target.z} - eye);
    const Vec3 right = normalize(cross(forward, Vec3{camera.up.x, camera.up.y, camera.up.z}));
    return {right, cross(right, forward)};
  }

  // One particle: a square facing the camera, turned around the line of sight.
  static void add_particle_quad(const Particle& particle, const Facing& view) {
    rlCheckRenderBatchLimit(kQuadCorners);
    const float half = particle.size_now() * kHalfFactor;
    const float turned_cos = std::cos(particle.angle);
    const float turned_sin = std::sin(particle.angle);
    const Vec3 across = (view.right * turned_cos + view.up * turned_sin) * half;
    const Vec3 along = (view.up * turned_cos - view.right * turned_sin) * half;
    const Vec3 corners[kQuadCorners] = {particle.position - across - along, particle.position + across - along,
                                        particle.position + across + along, particle.position - across + along};
    constexpr float u[kQuadCorners] = {0.0f, 1.0f, 1.0f, 0.0f};
    constexpr float v[kQuadCorners] = {1.0f, 1.0f, 0.0f, 0.0f};
    rlColor4f(particle.color.r, particle.color.g, particle.color.b, particle.alpha_now());
    for (int corner = 0; corner < kQuadCorners; ++corner) {
      rlTexCoord2f(u[corner], v[corner]);
      rlVertex3f(corners[corner].x, corners[corner].y, corners[corner].z);
    }
  }

  // The pictures a particle system uses, by Particle::image. Null for one the game's images
  // folder does not have: those particles are drawn as dots.
  std::vector<const Texture2D*> particle_pictures(const ParticleSystem& system) {
    std::vector<const Texture2D*> pictures;
    for (const std::string& name : system.images()) pictures.push_back(ui_image(name));
    return pictures;
  }

  // Draws the particles that glow (or the ones that do not) and wear `picture` (null: plain dots).
  void draw_particle_batch(const ParticleSystem& system, const std::vector<const Texture2D*>& pictures,
                           const Texture2D* picture, bool glowing, const Facing& view) {
    const float textured = picture != nullptr ? kOn : kOff;
    SetShaderValue(particle_shader, particle_textured_loc, &textured, SHADER_UNIFORM_FLOAT);
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

  // Particles are hidden behind objects but do not hide each other (no depth written). Ordinary
  // ones cover what is behind them; glowing ones add their light to it.
  void draw_particles(const ParticleSystem& system, const Camera3D& camera) {
    if (system.particles().empty()) return;
    const Facing view = facing(camera);
    const std::vector<const Texture2D*> pictures = particle_pictures(system);
    // The plain dots (a null picture), then each picture once.
    std::vector<const Texture2D*> batches{nullptr};
    for (const Texture2D* picture : pictures) {
      if (std::find(batches.begin(), batches.end(), picture) == batches.end()) batches.push_back(picture);
    }
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    BeginShaderMode(particle_shader);
    for (const bool glowing : {false, true}) {
      const float boost = glowing ? kParticleGlowBoost : kOn;
      SetShaderValue(particle_shader, particle_boost_loc, &boost, SHADER_UNIFORM_FLOAT);
      // The picture's own alpha is left alone: it is not see-through where a particle is.
      rlSetBlendFactorsSeparate(RL_SRC_ALPHA, glowing ? RL_ONE : RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD,
                                RL_FUNC_ADD);
      BeginBlendMode(BLEND_CUSTOM_SEPARATE);
      for (const Texture2D* picture : batches) draw_particle_batch(system, pictures, picture, glowing, view);
      EndBlendMode();
    }
    EndShaderMode();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
  }

  // A dome around the camera, behind everything (no depth written).
  void draw_sky(const Camera3D& camera) {
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    rlDisableDepthTest();
    DrawMesh(sky_mesh, sky_material, MatrixTranslate(camera.position.x, camera.position.y, camera.position.z));
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
  }
};

Renderer::Renderer(const RenderSettings& settings) : impl_(std::make_unique<Impl>()) {
  impl_->settings = settings;
  prefer_discrete_gpu();
  // raylib names the graphics card only at its most verbose level: listen while the window opens.
  SetTraceLogCallback(report_graphics_card);
  SetTraceLogLevel(LOG_INFO);
  // No multisampling: the scene is drawn into off-screen targets and smoothed by the FXAA pass.
  unsigned int flags = FLAG_VSYNC_HINT;
  if (settings.resizable) flags |= FLAG_WINDOW_RESIZABLE;
  SetConfigFlags(flags);
  InitWindow(settings.width, settings.height, settings.title.c_str());
  SetTraceLogCallback(nullptr);
  SetTraceLogLevel(LOG_WARNING);
  if (!IsWindowReady()) throw std::runtime_error("cannot open the game window");
  SetExitKey(KEY_NULL);  // games decide when to quit
  SetTargetFPS(config::kTargetFps);
  impl_->load_shader();
  impl_->load_ui_font();
}

Renderer::~Renderer() {
  impl_->release();
  CloseWindow();
}

void Renderer::upload(const SceneDesc& desc) {
  impl_->release_assets();
  impl_->faces_captured = 0;  // do not reflect the previous scene while the new one is captured
  sync(desc);
}

void Renderer::sync(const SceneDesc& desc) {
  // Assets are only ever appended to a scene, so upload whatever is new and refresh material values.
  for (std::size_t i = impl_->textures.size(); i < desc.images.size(); ++i) {
    const ImageData& image = desc.images[i];
    Texture2D texture{};
    if (!image.encoded.empty()) {
      Image decoded = LoadImageFromMemory(image.file_extension.c_str(), image.encoded.data(),
                                          static_cast<int>(image.encoded.size()));
      if (IsImageValid(decoded)) {
        texture = LoadTextureFromImage(decoded);
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);  // smooth, and it is what makes the mipmaps used
        SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);  // floors stay sharp at grazing angles
      }
      UnloadImage(decoded);
    }
    impl_->textures.push_back(texture);
  }
  for (std::size_t i = impl_->materials.size(); i < desc.materials.size(); ++i) {
    Material material = impl_->make_material(desc.materials[i].base_color);
    const int image = desc.materials[i].base_color_image;
    if (image != kNoIndex && impl_->textures[image].id != 0) {
      material.maps[MATERIAL_MAP_ALBEDO].texture = impl_->textures[image];
    }
    const MaterialDesc& source = desc.materials[i];
    const std::pair<int, int> extra_maps[] = {{MATERIAL_MAP_EMISSION, source.emissive_image},
                                              {MATERIAL_MAP_NORMAL, source.normal_image},
                                              {MATERIAL_MAP_ROUGHNESS, source.metallic_roughness_image},
                                              {MATERIAL_MAP_OCCLUSION, source.occlusion_image}};
    for (const auto& [slot, texture] : extra_maps) {
      if (texture != kNoIndex && impl_->textures[texture].id != 0) material.maps[slot].texture = impl_->textures[texture];
    }
    impl_->materials.push_back(material);
  }
  for (std::size_t i = 0; i < desc.materials.size(); ++i) {
    impl_->materials[i].maps[MATERIAL_MAP_ALBEDO].color = to_raylib(desc.materials[i].base_color);
  }
  impl_->material_factors = desc.materials;
  impl_->skins = desc.skins;
  for (std::size_t m = impl_->meshes.size(); m < desc.meshes.size(); ++m) {
    const MeshDesc& mesh_desc = desc.meshes[m];
    std::vector<GpuPrimitive> primitives;
    for (const PrimitiveDesc& source : mesh_desc.primitives) {
      for (const PrimitiveDesc& primitive : split_primitive(source, config::kMaxVerticesPerMesh)) {
        std::string error;
        std::optional<Mesh> mesh = build_mesh(primitive, error);
        if (!mesh) {
          log::warning("mesh '" + mesh_desc.name + "': " + error);
          continue;
        }
        UploadMesh(&*mesh, false);
        primitives.push_back({*mesh, primitive.material});
      }
    }
    impl_->meshes.push_back(std::move(primitives));
    impl_->mesh_bounds.push_back(mesh_desc.bounds);
  }
}

void Renderer::refresh_mesh(const SceneDesc& desc, int mesh) {
  if (mesh < 0 || static_cast<std::size_t>(mesh) >= impl_->meshes.size() ||
      static_cast<std::size_t>(mesh) >= desc.meshes.size()) {
    return;
  }
  const MeshDesc& source = desc.meshes[static_cast<std::size_t>(mesh)];
  std::vector<GpuPrimitive>& parts = impl_->meshes[static_cast<std::size_t>(mesh)];
  for (std::size_t i = 0; i < std::min(parts.size(), source.primitives.size()); ++i) {
    const PrimitiveDesc& primitive = source.primitives[i];
    // Same vertices in the same order, only moved: write over the buffers already on the card.
    if (static_cast<int>(primitive.positions.size() / config::kFloatsPerPosition) != parts[i].mesh.vertexCount) continue;
    const int bytes = static_cast<int>(primitive.positions.size() * sizeof(float));
    UpdateMeshBuffer(parts[i].mesh, kPositionBuffer, primitive.positions.data(), bytes, 0);
    UpdateMeshBuffer(parts[i].mesh, kNormalBuffer, primitive.normals.data(), bytes, 0);
  }
  impl_->mesh_bounds[static_cast<std::size_t>(mesh)] = source.bounds;
}

Camera3D Renderer::scene_camera(const Scene& scene) const { return impl_->camera_for(scene); }

void Renderer::draw_world(const Scene& scene, const Camera3D& camera, const RenderTexture2D* output,
                          const ParticleSystem* particles) {
  const int width = output != nullptr ? output->texture.width : GetRenderWidth();
  const int height = output != nullptr ? output->texture.height : GetRenderHeight();
  if (width <= 0 || height <= 0) return;  // a minimized window: nothing to draw into
  impl_->ensure_targets(width, height);
  const Environment env = scene_environment(scene);
  const Vec3 viewer{camera.position.x, camera.position.y, camera.position.z};
  impl_->place_objects(scene);
  impl_->apply_lights(scene, env, viewer);
  const Vector3 view_position = camera.position;

  impl_->view_tan_half_fov = std::tan(camera.fovy * DEG2RAD * kHalfFactor);
  impl_->view_aspect = static_cast<float>(width) / static_cast<float>(height);
  impl_->draw_shadow_map(scene, camera, env);
  impl_->capture_reflection_face(scene, camera);
  SetShaderValue(impl_->shader, impl_->shader.locs[SHADER_LOC_VECTOR_VIEW], &view_position, SHADER_UNIFORM_VEC3);
  BeginTextureMode(impl_->hdr);
  ClearBackground(kBlack);
  BeginMode3D(camera);
  impl_->draw_sky(camera);
  impl_->draw_objects(scene, false, viewer);
  if (particles != nullptr) impl_->draw_particles(*particles, camera);
  EndMode3D();
  EndTextureMode();
  impl_->post_process(env, output);
}

void Renderer::draw_hud(const std::vector<HudItem>& hud) {
  for (const HudItem& item : hud) {
    const ::Color color = to_raylib(item.color);
    switch (item.kind) {
      case HudItem::Kind::Text:
        DrawTextEx(impl_->ui_font, item.text.c_str(), {static_cast<float>(item.x), static_cast<float>(item.y)},
                   static_cast<float>(item.size), static_cast<float>(item.size) * kUiLetterSpacing, color);
        break;
      case HudItem::Kind::Rect: {
        const Rectangle box{static_cast<float>(item.x), static_cast<float>(item.y), static_cast<float>(item.width),
                            static_cast<float>(item.height)};
        const float shorter = std::min(box.width, box.height);
        if (item.radius > 0.0f && shorter > 0.0f) {
          DrawRectangleRounded(box, std::min(item.radius * kDiameterPerRadius / shorter, 1.0f), kUiCornerSegments, color);
        } else {
          DrawRectangleRec(box, color);
        }
        break;
      }
      case HudItem::Kind::Image: {
        const Texture2D* picture = impl_->ui_image(item.text);
        if (picture == nullptr) break;
        // Missing sizes follow the picture's own shape.
        float width = static_cast<float>(item.width);
        float height = static_cast<float>(item.height);
        const float shape = static_cast<float>(picture->width) / static_cast<float>(picture->height);
        if (width <= 0.0f && height <= 0.0f) width = static_cast<float>(picture->width), height = static_cast<float>(picture->height);
        if (width <= 0.0f) width = height * shape;
        if (height <= 0.0f) height = width / shape;
        DrawTexturePro(*picture, {0.0f, 0.0f, static_cast<float>(picture->width), static_cast<float>(picture->height)},
                       {static_cast<float>(item.x), static_cast<float>(item.y), width, height}, {0.0f, 0.0f}, 0.0f, color);
        break;
      }
    }
  }
}

void Renderer::set_motion_blur(float amount) { impl_->motion_blur = std::clamp(amount, 0.0f, 1.0f); }

void Renderer::set_images_dir(const std::filesystem::path& dir) {
  impl_->release_ui_images();
  impl_->images_dir = dir;
}

int Renderer::text_width(const std::string& text, int size) const {
  const float text_size = static_cast<float>(size);
  return static_cast<int>(std::ceil(MeasureTextEx(impl_->ui_font, text.c_str(), text_size, text_size * kUiLetterSpacing).x));
}

int Renderer::screen_width() const { return GetRenderWidth(); }
int Renderer::screen_height() const { return GetRenderHeight(); }

void Renderer::set_cursor_locked(bool locked) {
  if (locked == IsCursorHidden()) return;
  if (locked) {
    DisableCursor();
  } else {
    EnableCursor();
  }
}

void Renderer::draw_touch_controls(const TouchLayout& controls) {
  const auto circle = [](const TouchCircle& at, unsigned char resting, unsigned char pressed) {
    const ::Color fill{kTouchShade, kTouchShade, kTouchShade, at.pressed ? pressed : resting};
    DrawCircleV({at.x, at.y}, at.radius, fill);
  };
  circle(controls.stick_base, kTouchBaseAlpha, kTouchBaseAlpha);
  circle(controls.stick_knob, kTouchRestingAlpha, kTouchPressedAlpha);
  for (const TouchCircle* button : {&controls.a, &controls.b, &controls.start}) {
    circle(*button, kTouchRestingAlpha, kTouchPressedAlpha);
  }
}

void Renderer::draw(const Scene& scene, const std::vector<HudItem>& hud,
                    const std::vector<std::filesystem::path>& screenshot_paths, const ParticleSystem* particles,
                    const TouchLayout* touch) {
  BeginDrawing();
  draw_world(scene, impl_->camera_for(scene), nullptr, particles);
  draw_hud(hud);
  if (touch != nullptr) draw_touch_controls(*touch);
  if (impl_->settings.show_fps) DrawFPS(config::kFpsCounterX, config::kFpsCounterY);

  if (!screenshot_paths.empty()) {
    rlDrawRenderBatchActive();
    Image frame = LoadImageFromScreen();
    for (const std::filesystem::path& path : screenshot_paths) {
      std::filesystem::create_directories(path.parent_path());
      if (!ExportImage(frame, path.string().c_str())) log::error("cannot write screenshot " + path.string());
    }
    UnloadImage(frame);
  }
  EndDrawing();
}

void Renderer::set_show_prefabs(bool show) { impl_->show_prefabs = show; }

bool Renderer::close_requested() const { return WindowShouldClose(); }

float Renderer::frame_seconds() const { return GetFrameTime(); }

}  // namespace redgem
