#include "render/renderer.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <utility>

#include "core/config.hpp"
#include "core/log.hpp"
#include "render/culling.hpp"
#include "render/environment.hpp"
#include "render/gpu_select.hpp"
#include "render/mesh_builder.hpp"
#include "render/shaders.hpp"

namespace redgem {
namespace {

constexpr float kHalfFactor = 0.5f;
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

struct Renderer::Impl {
  RenderSettings settings;
  Shader shader{};
  Shader depth_shader{};
  Shader sky_shader{};
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
  RenderTexture2D bloom_a{};  // bright parts, blurred (ping-pong with bloom_b)
  RenderTexture2D bloom_b{};
  RenderTexture2D ldr{};      // tone mapped, luma in alpha, before anti-aliasing
  RenderTexture2D shadow{};   // depth of the scene seen from the sun (no color)
  float wind = 0.0f;          // of the environment being drawn
  Vector3 sun_travel{};       // direction the sun's light travels this frame

  static Shader compile(const char* vertex, const char* fragment, const char* name) {
    const Shader compiled = LoadShaderFromMemory(vertex, fragment);
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
    if (shadow.id != 0) {
      rlUnloadTexture(shadow.depth.id);
      rlUnloadFramebuffer(shadow.id);
      shadow = RenderTexture2D{};
    }
    for (RenderTexture2D* target : {&reflection_face, &panorama}) {
      if (target->id != 0) UnloadRenderTexture(*target);
      *target = RenderTexture2D{};
    }
    for (Shader* loaded : {&shader, &depth_shader, &panorama_shader, &occlusion_shader, &sky_shader, &bright_shader, &blur_shader, &composite_shader, &antialias_shader}) {
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
                                        model_matrix(scene.world_transform(object.joints[j])));
    }
    return count > 0;
  }

  // Draws what the current 3D view can see. `viewer` is the game camera in both passes: detail
  // is judged by how big things look to the player, not to the sun.
  void draw_objects(const Scene& scene, bool depth_pass, Vec3 viewer, float detail_angle = 0.0f) {
    const Frustum frustum = frustum_from_matrix(MatrixToFloatV(MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection())).v);
    const float min_angle = detail_angle > 0.0f ? detail_angle : depth_pass ? kMinShadowAngle : kMinVisibleAngle;
    for (const ObjectId id : scene.alive_ids()) {
      const GameObject& object = scene.at(id);
      if (object.mesh == kNoIndex || !object.visible || (object.prefab && !show_prefabs)) continue;
      if (static_cast<std::size_t>(object.mesh) >= meshes.size()) continue;
      const Transform world = scene.world_transform(id);
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
      }
      const Matrix transform = model_matrix(world);

      if (!depth_pass) {
        const Vector3 tint = to_raylib(object.tint);
        SetShaderValue(shader, tint_loc, &tint, SHADER_UNIFORM_VEC3);
      }
      for (const GpuPrimitive& primitive : meshes[object.mesh]) {
        Material material = primitive.material == kNoIndex ? fallback_material : materials[primitive.material];
        if (depth_pass) material.shader = depth_shader;
        const bool two_sided = apply_material(primitive.material, depth_pass);
        if (two_sided) rlDisableBackfaceCulling();
        // Per part: a skinned object may still have parts without bone weights, which stay rigid.
        Mesh mesh = primitive.mesh;
        const bool deformed = skinned && mesh.boneIds != nullptr;
        if (deformed) {
          mesh.boneMatrices = bone_matrices.data();
          mesh.boneCount = static_cast<int>(bone_matrices.size());
        }
        const float skinning = deformed ? kOn : kOff;
        SetShaderValue(depth_pass ? depth_shader : shader, depth_pass ? depth_skinned_loc : skinned_loc, &skinning,
                       SHADER_UNIFORM_FLOAT);
        DrawMesh(mesh, material, transform);
        if (two_sided) rlEnableBackfaceCulling();
      }
    }
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

Camera3D Renderer::scene_camera(const Scene& scene) const { return impl_->camera_for(scene); }

void Renderer::draw_world(const Scene& scene, const Camera3D& camera, const RenderTexture2D* output) {
  const int width = output != nullptr ? output->texture.width : GetRenderWidth();
  const int height = output != nullptr ? output->texture.height : GetRenderHeight();
  if (width <= 0 || height <= 0) return;  // a minimized window: nothing to draw into
  impl_->ensure_targets(width, height);
  const Environment env = scene_environment(scene);
  const Vec3 viewer{camera.position.x, camera.position.y, camera.position.z};
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
  EndMode3D();
  EndTextureMode();
  impl_->post_process(env, output);
}

void Renderer::draw_hud(const std::vector<HudText>& hud) {
  for (const HudText& text : hud) {
    DrawText(text.text.c_str(), text.x, text.y, text.size, to_raylib(text.color));
  }
}

void Renderer::draw(const Scene& scene, const std::vector<HudText>& hud,
                    const std::vector<std::filesystem::path>& screenshot_paths) {
  BeginDrawing();
  draw_world(scene, impl_->camera_for(scene));
  draw_hud(hud);
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
