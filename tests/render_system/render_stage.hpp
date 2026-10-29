#pragma once

// A window and a renderer for scenes made in code, and what they drew, as pixels a test can read.

#include <raylib.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "render/renderer.hpp"
#include "render/window.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::test {

// A picture the renderer drew.
struct Picture {
  int width = 0;
  int height = 0;
  std::vector<::Color> pixels;  // row by row

  // Where the pixels `wanted` picks are, on average, across the picture (0 at its left, 1 at its
  // right), and how many there are.
  struct Found {
    float across = 0.0f;
    std::size_t count = 0;
  };
  Found find(const std::function<bool(::Color)>& wanted) const;
  ::Color at(float across, float down) const;
};

class RenderStage {
 public:
  RenderStage();
  ~RenderStage();
  RenderStage(const RenderStage&) = delete;
  RenderStage& operator=(const RenderStage&) = delete;

  // Uploads a scene's meshes and materials, replacing the last one's.
  void set_clock(double seconds) { renderer_->set_clock(seconds); }
  void upload(const SceneDesc& desc) { renderer_->upload(desc); }
  // Brings the uploaded scene up to date with what changed in `desc` since, as the editor does
  // every frame (new pictures and materials, and their values).
  void sync(const SceneDesc& desc) { renderer_->sync(desc); }
  // Where the scene's pictures (a sky picture) and its particles' are looked for.
  void set_images_dir(const std::filesystem::path& dir) { renderer_->set_images_dir(dir); }
  // The scene as its camera sees it, with `particles` when given; also written to
  // <screenshots>/render/<name>.png for whoever looks at the run afterwards.
  Picture draw(const Scene& scene, const std::string& name, const ParticleSystem* particles = nullptr,
               const WaterRippleSnapshot* water_ripples = nullptr);

  // Unjittered debug test scenes with depth and x-ray commands over their opaque objects.
  Picture draw_debug(const Scene& scene, const DebugDrawBuffer& commands, const std::string& name);
  // Default-screen readback before EndDrawing flushes: draw_world must finish its native quad.
  Picture draw_debug_native(const Scene& scene, const DebugDrawBuffer& commands, const std::string& name);
  // Flat UI drawn into the same target, for testing images and text.
  Picture draw_hud(const std::vector<HudItem>& hud, const std::string& name);

 private:
  Picture capture(const std::string& name);
  Window window_;
  std::unique_ptr<Renderer> renderer_;
  RenderTexture2D target_{};
};

// A camera node at `position`, looking at the origin.
NodeDesc camera_at(Vec3 position);
// A cube a meter across, drawn with `material`.
MeshDesc cube_mesh(int material);
// A material that shines with its own color, `glow` times over: lit the same whatever the light.
MaterialDesc glowing_material(Color color, float glow);

}  // namespace redgem::test
