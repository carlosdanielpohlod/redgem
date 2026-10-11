// Loading and releasing what the renderer keeps on the graphics card.

#include "render/renderer_impl.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

#include "core/log.hpp"
#include "editor/ui/embedded_fonts.hpp"
#include "render/mesh_builder.hpp"
#include "render/shader_source.hpp"
#include "render/shaders.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::downscaled;
using render_math::to_raylib;

// Sky dome (around the camera; its size only has to fit between the near and far planes).
constexpr float kSkyRadius = 50.0f;  // meters
constexpr int kSkyRings = 16;
constexpr int kSkySlices = 32;
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
// Texts drawn by games (UI.text): rasterized at this height and scaled to the size asked for.
constexpr int kUiFontPixels = 72;
constexpr int kUiFirstLetter = 32;  // space
constexpr int kUiLastLetter = 255;  // through Latin-1: accented letters
constexpr const char* kFontFileType = ".ttf";
// How a picture's file may be named: as asked for, or with one of the kinds of picture read.
constexpr const char* kPictureExtensions[] = {"", ".png", ".jpg", ".jpeg"};

// The uniforms raylib has a place for, under the lit shader's own names for them.
constexpr std::pair<int, const char*> kKnownUniforms[] = {
    {SHADER_LOC_MATRIX_MODEL, shaders::kModelMatrix},   {SHADER_LOC_MATRIX_NORMAL, shaders::kNormalMatrix},
    {SHADER_LOC_VECTOR_VIEW, shaders::kViewPosition},   {SHADER_LOC_MAP_EMISSION, shaders::kEmissiveMap},
    {SHADER_LOC_MAP_NORMAL, shaders::kNormalMap},       {SHADER_LOC_MAP_ROUGHNESS, shaders::kSurfaceMap},
    {SHADER_LOC_MAP_OCCLUSION, shaders::kOcclusionMap}, {SHADER_LOC_BONE_MATRICES, shaders::kBoneMatrices},
};

// One part of a mesh, on the graphics card. A part that cannot be drawn is reported and left out.
std::optional<GpuPrimitive> upload_primitive(const PrimitiveDesc& primitive, const std::string& mesh_name) {
  std::string error;
  std::optional<Mesh> mesh = build_mesh(primitive, error);
  if (!mesh) {
    log::warning("mesh '" + mesh_name + "': " + error);
    return std::nullopt;
  }
  UploadMesh(&*mesh, false);
  return GpuPrimitive{*mesh, primitive.material};
}

// The codes of the letters the game's font holds.
std::vector<int> ui_letters() {
  std::vector<int> letters;
  for (int code = kUiFirstLetter; code <= kUiLastLetter; ++code) letters.push_back(code);
  return letters;
}

void unload(RenderTexture2D& target) {
  if (target.id != 0) UnloadRenderTexture(target);
  target = RenderTexture2D{};
}

// A material made by LoadMaterialDefault owns only its array of maps.
void free_maps(Material& material) {
  if (material.maps != nullptr) MemFree(material.maps);
  material = Material{};
}

}  // namespace

// --- Shaders ----------------------------------------------------------------------------------

// What the lit shader takes per object and per material.
const Renderer::Impl::Uniform Renderer::Impl::kMaterialUniforms[] = {
    {&Impl::has_normal_map_loc, shaders::kHasNormalMap},
    {&Impl::normal_scale_loc, shaders::kNormalScale},
    {&Impl::has_surface_map_loc, shaders::kHasSurfaceMap},
    {&Impl::occlusion_strength_loc, shaders::kOcclusionStrength},
    {&Impl::metallic_loc, shaders::kMetallic},
    {&Impl::roughness_loc, shaders::kRoughness},
    {&Impl::emissive_loc, shaders::kEmissive},
    {&Impl::has_emissive_map_loc, shaders::kHasEmissiveMap},
    {&Impl::alpha_cutoff_loc, shaders::kAlphaCutoff},
    {&Impl::tint_loc, shaders::kTint},
    {&Impl::skinned_loc, shaders::kSkinned},
    {&Impl::wind_loc, shaders::kWindAmount},
};

// What the lit shader takes once per frame: lights, shadows, reflections and the sky.
const Renderer::Impl::Uniform Renderer::Impl::kFrameUniforms[] = {
    {&Impl::shadow_map_loc, shaders::kShadowMap},
    {&Impl::light_matrix_loc, shaders::kLightViewProjection},
    {&Impl::shadow_strength_loc, shaders::kShadowStrength},
    {&Impl::shadow_texel_loc, shaders::kShadowTexel},
    {&Impl::reflection_map_loc, shaders::kReflectionMap},
    {&Impl::reflection_strength_loc, shaders::kReflectionStrength},
    {&Impl::sun_direction_loc, shaders::kSunDirection},
    {&Impl::sun_color_loc, shaders::kSunColor},
    {&Impl::ambient_loc, shaders::kAmbientStrength},
    {&Impl::point_count_loc, shaders::kPointLightCount},
    {&Impl::point_positions_loc, shaders::kPointLightPositions},
    {&Impl::point_colors_loc, shaders::kPointLightColors},
    {&Impl::point_ranges_loc, shaders::kPointLightRanges},
    {&Impl::fog_color_loc, shaders::kFogColor},
    {&Impl::fog_density_loc, shaders::kFogDensity},
    {&Impl::time_loc, shaders::kTime},
    {&Impl::sky_top_loc, shaders::kSkyTop},
    {&Impl::sky_horizon_loc, shaders::kSkyHorizon},
    {&Impl::ground_loc, shaders::kGroundColor},
};

Shader Renderer::Impl::compile(const char* vertex, const char* fragment, const char* name) {
  const std::string vertex_text = vertex != nullptr ? shaders::for_this_gpu(vertex) : std::string();
  const std::string fragment_text = fragment != nullptr ? shaders::for_this_gpu(fragment) : std::string();
  const Shader compiled = LoadShaderFromMemory(vertex != nullptr ? vertex_text.c_str() : nullptr,
                                               fragment != nullptr ? fragment_text.c_str() : nullptr);
  if (!IsShaderValid(compiled)) throw std::runtime_error(std::string("failed to compile the ") + name + " shader");
  return compiled;
}

void Renderer::Impl::load_pipeline() {
  compile_shaders();
  find_material_uniforms();
  find_light_uniforms();
  find_depth_and_particle_uniforms();
  load_fixed_targets();
  fallback_material = make_material(redgem::Color{});
  load_sky();
}

// (Full-screen passes have no vertex shader of their own: raylib's default one does.)
void Renderer::Impl::compile_shaders() {
  shader = compile(shaders::kLitVertex, shaders::kLitFragment, "lit");
  depth_shader = compile(shaders::kLitVertex, shaders::kDepthFragment, "shadow depth");
  sky_shader = compile(shaders::kSkyVertex, shaders::kSkyFragment, "sky");
  particle_shader = compile(nullptr, shaders::kParticleFragment, "particles");
  bright_shader = compile(nullptr, shaders::kBrightFragment, "bloom threshold");
  blur_shader = compile(nullptr, shaders::kBlurFragment, "blur");
  composite_shader = compile(nullptr, shaders::kCompositeFragment, "tone mapping");
  antialias_shader = compile(nullptr, shaders::kAntialiasFragment, "anti-aliasing");
  panorama_shader = compile(nullptr, shaders::kPanoramaFragment, "reflection panorama");
  occlusion_shader = compile(nullptr, shaders::kAmbientOcclusionFragment, "ambient occlusion");
}

// What the lit shader takes per object and per material.
void Renderer::Impl::find_material_uniforms() {
  for (const auto& [place, name] : kKnownUniforms) shader.locs[place] = GetShaderLocation(shader, name);
  for (const Uniform& uniform : kMaterialUniforms) this->*uniform.location = GetShaderLocation(shader, uniform.name);
}

// What the lit shader takes once per frame: lights, shadows, reflections and the sky.
void Renderer::Impl::find_light_uniforms() {
  for (const Uniform& uniform : kFrameUniforms) this->*uniform.location = GetShaderLocation(shader, uniform.name);
}

void Renderer::Impl::find_depth_and_particle_uniforms() {
  depth_shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(depth_shader, shaders::kModelMatrix);
  depth_shader.locs[SHADER_LOC_BONE_MATRICES] = GetShaderLocation(depth_shader, shaders::kBoneMatrices);
  depth_alpha_cutoff_loc = GetShaderLocation(depth_shader, shaders::kAlphaCutoff);
  depth_wind_loc = GetShaderLocation(depth_shader, shaders::kWindAmount);
  depth_time_loc = GetShaderLocation(depth_shader, shaders::kTime);
  depth_skinned_loc = GetShaderLocation(depth_shader, shaders::kSkinned);
  particle_boost_loc = GetShaderLocation(particle_shader, shaders::kParticleBoost);
  particle_textured_loc = GetShaderLocation(particle_shader, shaders::kParticleTextured);
}

// The targets whose size does not follow the window: the shadow map and the reflections.
void Renderer::Impl::load_fixed_targets() {
  shadow = load_shadow_target(kShadowMapSize);
  reflection_face = load_target(kReflectionFaceSize, kReflectionFaceSize, kHdrFormat, true);
  panorama = load_target(kPanoramaWidth, kPanoramaHeight, kHdrFormat, false);
  // Around the horizon the panorama wraps; at the poles it must not (up would blend with down).
  rlTextureParameters(panorama.texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
  rlTextureParameters(panorama.texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
}

void Renderer::Impl::load_sky() {
  sky_mesh = GenMeshSphere(kSkyRadius, kSkyRings, kSkySlices);
  sky_material = LoadMaterialDefault();
  sky_material.shader = sky_shader;
}

Material Renderer::Impl::make_material(redgem::Color base_color) {
  Material material = LoadMaterialDefault();
  material.shader = shader;
  material.maps[MATERIAL_MAP_ALBEDO].color = to_raylib(base_color);
  return material;
}

// --- Releasing --------------------------------------------------------------------------------

void Renderer::Impl::release() {
  release_assets();
  release_targets();
  release_sky();
  release_ui_images();
  if (owns_ui_font) UnloadFont(ui_font);
  owns_ui_font = false;
  release_fixed_targets();
  release_shaders();
}

void Renderer::Impl::release_assets() {
  release_meshes();
  skins.clear();
  for (Texture2D& texture : textures) UnloadTexture(texture);
  textures.clear();
  // Materials share the shader and textures released above; only free their map arrays.
  for (Material& material : materials) MemFree(material.maps);
  materials.clear();
  material_factors.clear();
}

void Renderer::Impl::release_meshes() {
  for (auto& primitives : meshes) {
    for (GpuPrimitive& primitive : primitives) UnloadMesh(primitive.mesh);
  }
  meshes.clear();
  mesh_bounds.clear();
}

void Renderer::Impl::release_sky() {
  free_maps(fallback_material);
  free_maps(sky_material);
  if (sky_mesh.vaoId != 0) UnloadMesh(sky_mesh);
  sky_mesh = Mesh{};
}

void Renderer::Impl::release_fixed_targets() {
  if (shadow.id != 0) {
    rlUnloadTexture(shadow.depth.id);
    rlUnloadFramebuffer(shadow.id);
    shadow = RenderTexture2D{};
  }
  for (RenderTexture2D* target : {&reflection_face, &panorama}) unload(*target);
}

void Renderer::Impl::release_shaders() {
  for (Shader* loaded : {&shader, &depth_shader, &panorama_shader, &occlusion_shader, &sky_shader, &particle_shader,
                         &bright_shader, &blur_shader, &composite_shader, &antialias_shader}) {
    if (loaded->id != 0) UnloadShader(*loaded);
    *loaded = Shader{};
  }
}

void Renderer::Impl::release_targets() {
  for (RenderTexture2D* target : {&hdr, &bloom_a, &bloom_b, &ldr, &occlusion_a, &occlusion_b}) unload(*target);
}

void Renderer::Impl::release_ui_images() {
  for (auto& [name, picture] : ui_images) {
    if (picture.id != 0) UnloadTexture(picture);
  }
  ui_images.clear();
}

// --- Render targets ---------------------------------------------------------------------------

// A depth texture (not a render buffer: the ambient occlusion pass and the lit shader read
// depth), attached to the framebuffer being set up.
Texture2D Renderer::Impl::load_depth_texture(unsigned int framebuffer, int width, int height) {
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
Texture2D Renderer::Impl::load_color_texture(unsigned int framebuffer, int width, int height, int format) {
  Texture2D color{};
  color.id = rlLoadTexture(nullptr, width, height, format, kOneMipLevel);
  color.width = width;
  color.height = height;
  color.format = format;
  color.mipmaps = kOneMipLevel;
  rlFramebufferAttach(framebuffer, color.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
  return color;
}

RenderTexture2D Renderer::Impl::load_target(int width, int height, int format, bool with_depth) {
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

RenderTexture2D Renderer::Impl::load_shadow_target(int size) {
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

void Renderer::Impl::ensure_targets(int width, int height) {
  if (hdr.id != 0 && hdr.texture.width == width && hdr.texture.height == height) return;
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

// --- The scene's assets -----------------------------------------------------------------------

Texture2D Renderer::Impl::load_texture(const ImageData& image) {
  Texture2D texture{};
  if (image.encoded.empty()) return texture;
  Image decoded =
      LoadImageFromMemory(image.file_extension.c_str(), image.encoded.data(), static_cast<int>(image.encoded.size()));
  if (IsImageValid(decoded)) {
    texture = LoadTextureFromImage(decoded);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);       // smooth, and it is what makes the mipmaps used
    SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);  // floors stay sharp at grazing angles
  }
  UnloadImage(decoded);
  return texture;
}

void Renderer::Impl::upload_new_textures(const SceneDesc& desc) {
  for (std::size_t i = textures.size(); i < desc.images.size(); ++i) textures.push_back(load_texture(desc.images[i]));
}

// A material with the textures it uses. (An image that could not be decoded leaves its map empty.)
Material Renderer::Impl::load_material(const MaterialDesc& source) {
  Material material = make_material(source.base_color);
  const std::pair<int, int> maps[] = {{MATERIAL_MAP_ALBEDO, source.base_color_image},
                                      {MATERIAL_MAP_EMISSION, source.emissive_image},
                                      {MATERIAL_MAP_NORMAL, source.normal_image},
                                      {MATERIAL_MAP_ROUGHNESS, source.metallic_roughness_image},
                                      {MATERIAL_MAP_OCCLUSION, source.occlusion_image}};
  for (const auto& [slot, image] : maps) {
    if (image != kNoIndex && textures[image].id != 0) material.maps[slot].texture = textures[image];
  }
  return material;
}

void Renderer::Impl::upload_new_materials(const SceneDesc& desc) {
  for (std::size_t i = materials.size(); i < desc.materials.size(); ++i) {
    materials.push_back(load_material(desc.materials[i]));
  }
}

void Renderer::Impl::refresh_material_values(const SceneDesc& desc) {
  for (std::size_t i = 0; i < desc.materials.size(); ++i) {
    materials[i].maps[MATERIAL_MAP_ALBEDO].color = to_raylib(desc.materials[i].base_color);
  }
  material_factors = desc.materials;
  skins = desc.skins;
}

// The parts of a mesh, on the graphics card.
std::vector<GpuPrimitive> Renderer::Impl::upload_mesh(const MeshDesc& mesh_desc) {
  std::vector<GpuPrimitive> primitives;
  for (const PrimitiveDesc& source : mesh_desc.primitives) {
    for (const PrimitiveDesc& primitive : split_primitive(source, config::kMaxVerticesPerMesh)) {
      if (const auto uploaded = upload_primitive(primitive, mesh_desc.name)) primitives.push_back(*uploaded);
    }
  }
  return primitives;
}

void Renderer::Impl::upload_new_meshes(const SceneDesc& desc) {
  for (std::size_t m = meshes.size(); m < desc.meshes.size(); ++m) {
    meshes.push_back(upload_mesh(desc.meshes[m]));
    mesh_bounds.push_back(desc.meshes[m].bounds);
  }
}

// --- Texts and pictures -----------------------------------------------------------------------

void Renderer::Impl::load_ui_font() {
  const fonts::EmbeddedFont inter = fonts::interface_font();
  std::vector<int> letters = ui_letters();  // (not const: raylib takes them as plain int*)
  ui_font = LoadFontFromMemory(kFontFileType, inter.data, inter.size, kUiFontPixels, letters.data(),
                               static_cast<int>(letters.size()));
  if (ui_font.texture.id == 0) {
    ui_font = GetFontDefault();
    return;
  }
  GenTextureMipmaps(&ui_font.texture);
  SetTextureFilter(ui_font.texture, TEXTURE_FILTER_TRILINEAR);
  owns_ui_font = true;
}

// The first file of the images folder that `name` can mean, as a texture (id 0 when none does).
Texture2D Renderer::Impl::load_picture(const std::string& name) const {
  for (const char* extension : kPictureExtensions) {
    const std::filesystem::path file = images_dir / (name + extension);
    std::error_code fs_error;
    if (std::filesystem::is_regular_file(file, fs_error)) return LoadTexture(file.string().c_str());
  }
  return Texture2D{};
}

const Texture2D* Renderer::Impl::ui_image(const std::string& name) {
  const auto found = ui_images.find(name);
  if (found != ui_images.end()) return found->second.id != 0 ? &found->second : nullptr;
  const Texture2D picture = load_picture(name);
  if (picture.id == 0) {
    log::error("no picture named '" + name + "' in " + images_dir.string());
  } else {
    SetTextureFilter(picture, TEXTURE_FILTER_BILINEAR);
  }
  const Texture2D& kept = ui_images.emplace(name, picture).first->second;
  return kept.id != 0 ? &kept : nullptr;
}

}  // namespace redgem
