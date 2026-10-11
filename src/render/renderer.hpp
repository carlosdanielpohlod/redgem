#pragma once

#include <raylib.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"
#include "script/script_context.hpp"

namespace redgem {

struct RenderSettings {
  int width = 0;
  int height = 0;
  std::string title;
  bool show_fps = true;
  bool resizable = false;
};

// Owns the window and every GPU resource. Only one instance may exist at a time.
class Renderer {
 public:
  explicit Renderer(const RenderSettings& settings);
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  // Uploads meshes, materials and textures, replacing the previous scene's assets.
  void upload(const SceneDesc& desc);
  // Uploads assets added to `desc` since the last upload/sync and refreshes every material's values
  // (cheap enough to call every frame in the editor).
  void sync(const SceneDesc& desc);
  // Uploads again the vertices of a mesh that was reshaped (a sculpted terrain).
  void refresh_mesh(const SceneDesc& desc, int mesh);
  // Camera of the scene's main camera object (or a default one).
  Camera3D scene_camera(const Scene& scene) const;
  // Draws the scene with its environment (sky, fog) and post-processing (bloom, tone mapping,
  // vignette, anti-aliasing), without HUD, into `output` or onto the screen when null. Must not
  // be called between BeginTextureMode/EndTextureMode. The scene's depth is copied to `output`,
  // so overlays drawn into it afterwards are hidden behind objects.
  void draw_world(const Scene& scene, const Camera3D& camera, const RenderTexture2D* output = nullptr);
  // Draws what scripts put on screen this frame: texts, rectangles and images.
  void draw_hud(const std::vector<HudItem>& hud);
  // Where UI.image looks for pictures (the game's images/ folder).
  void set_images_dir(const std::filesystem::path& dir);
  // Width in pixels of a text drawn by draw_hud at that size.
  int text_width(const std::string& text, int size) const;
  // Size of the window's drawing area.
  int screen_width() const;
  int screen_height() const;
  // Hides the pointer and keeps it in the window (first-person cameras), or gives it back.
  void set_cursor_locked(bool locked);
  // Prefabs (spawn templates) are hidden in games and shown in the editor.
  void set_show_prefabs(bool show);
  // Draws one full game frame. Screenshots requested for this frame are written to `screenshot_paths`.
  void draw(const Scene& scene, const std::vector<HudItem>& hud,
            const std::vector<std::filesystem::path>& screenshot_paths);

  bool close_requested() const;
  float frame_seconds() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace redgem
