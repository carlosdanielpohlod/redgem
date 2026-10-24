#include "render_system/render_stage.hpp"

#include "scene/shapes/primitives.hpp"

namespace redgem::test {
namespace {

constexpr ::Color kHudBackground{255, 255, 255, 255};
constexpr int kSide = 256;  // pixels across the picture, and down
constexpr const char* kTitle = "RedGem render test";

std::filesystem::path picture_file(const std::string& name) {
  return std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "render" / (name + ".png");
}

void keep_picture(const Image& image, const std::string& name) {
  const std::filesystem::path file = picture_file(name);
  std::filesystem::create_directories(file.parent_path());
  ExportImage(image, file.string().c_str());
}

Picture picture_of(const Image& image) {
  Picture picture{image.width, image.height, {}};
  const auto* colors = static_cast<const ::Color*>(image.data);
  picture.pixels.assign(colors,
                        colors + static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height));
  return picture;
}

}  // namespace

Picture::Found Picture::find(const std::function<bool(::Color)>& wanted) const {
  Found found;
  double sum = 0.0;
  for (std::size_t i = 0; i < pixels.size(); ++i) {
    if (!wanted(pixels[i])) continue;
    sum += static_cast<double>(i % static_cast<std::size_t>(width)) / width;
    ++found.count;
  }
  if (found.count > 0) found.across = static_cast<float>(sum / static_cast<double>(found.count));
  return found;
}

::Color Picture::at(float across, float down) const {
  const int x = static_cast<int>(across * static_cast<float>(width - 1));
  const int y = static_cast<int>(down * static_cast<float>(height - 1));
  return pixels[static_cast<std::size_t>(y * width + x)];
}

RenderStage::RenderStage() : window_(WindowSettings{kSide, kSide, kTitle}) {
  renderer_ = std::make_unique<Renderer>(window_);
  target_ = LoadRenderTexture(kSide, kSide);
}

RenderStage::~RenderStage() {
  UnloadRenderTexture(target_);
  renderer_.reset();  // before the window it draws in
}

Picture RenderStage::draw(const Scene& scene, const std::string& name, const ParticleSystem* particles,
                          const WaterRippleSnapshot* water_ripples) {
  BeginDrawing();
  renderer_->draw_world(scene, renderer_->scene_camera(scene), &target_, particles, water_ripples);
  EndDrawing();
  return capture(name);
}

Picture RenderStage::draw_hud(const std::vector<HudItem>& hud, const std::string& name) {
  BeginDrawing();
  BeginTextureMode(target_);
  ClearBackground(kHudBackground);
  renderer_->draw_hud(hud);
  EndTextureMode();
  EndDrawing();
  return capture(name);
}

Picture RenderStage::capture(const std::string& name) {
  Image image = LoadImageFromTexture(target_.texture);
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  ImageFlipVertical(&image);  // render textures are stored upside down
  const Picture picture = picture_of(image);
  keep_picture(image, name);
  UnloadImage(image);
  return picture;
}

NodeDesc camera_at(Vec3 position) {
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  camera.local.position = position;
  camera.local.rotation = quat_look_rotation(Vec3::zero() - position, Vec3::up());
  return camera;
}

MeshDesc cube_mesh(int material) {
  MeshDesc mesh{"Cube", {make_primitive(PrimitiveShape::Cube)}, {}};
  mesh.primitives[0].material = material;
  for (std::size_t i = 0; i < mesh.primitives[0].positions.size(); i += 3) {
    mesh.bounds.include(vec3_from(&mesh.primitives[0].positions[i]));
  }
  return mesh;
}

MaterialDesc glowing_material(Color color, float glow) {
  MaterialDesc material;
  material.base_color = color;
  material.emissive = Vec3{color.r, color.g, color.b} * glow;
  return material;
}

}  // namespace redgem::test
