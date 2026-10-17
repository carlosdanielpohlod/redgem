#pragma once

#include <raylib.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "render/hud.hpp"
#include "scene/particles.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

class Window;

// Owns every GPU resource a game is drawn with (shaders, targets, meshes, textures, fonts,
// pictures) and draws with them. It draws in whatever window is open: that window is somebody
// else's, and has to outlive the renderer. Only one instance may exist at a time.
class Renderer {
 public:
  // `window` is the open window it draws in: asked for so that there is one.
  explicit Renderer(const Window& window);
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
  // `particles`, when given, are drawn over the objects (and behind the ones in front of them).
  void draw_world(const Scene& scene, const Camera3D& camera, const RenderTexture2D* output = nullptr,
                  const ParticleSystem* particles = nullptr);
  // Draws what scripts put on screen this frame: texts, rectangles and images.
  void draw_hud(const std::vector<HudItem>& hud);
  // How much the picture streaks with speed, 0 (none) to 1 (see Camera.motion_blur in Ruby).
  void set_motion_blur(float amount);
  // Whether environments that ask for temporal anti-aliasing get it (on unless said): a view whose
  // overlays are drawn with the camera unjittered afterwards (the editor's) does without.
  void allow_temporal_antialias(bool allowed);
  // The clock what moves with time (wind, film grain) follows, in seconds; nothing for the real
  // one. A recording sets it to the game's, which advances a fixed step every frame.
  void set_clock(std::optional<double> seconds);
  // Where UI.image looks for pictures (the game's images/ folder).
  void set_images_dir(const std::filesystem::path& dir);
  // Width in pixels of a text drawn by draw_hud at that size.
  int text_width(const std::string& text, int size) const;
  // Prefabs (spawn templates) are hidden in games and shown in the editor.
  void set_show_prefabs(bool show);
  // Draws a touch screen's controls over everything: see-through, brighter where a finger is.
  void draw_controls(const std::vector<ControlCircle>& controls);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace redgem
