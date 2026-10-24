#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "render/hud.hpp"
#include "render/window.hpp"
#include "scene/particles.hpp"
#include "scene/water_ripples.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// The number of the scene a game starts in; each scene it loads afterwards gets the next one.
inline constexpr int kFirstSceneGeneration = 0;

// Everything it takes to draw one frame of a game. The engine fills one per frame and hands it to
// its presenter; whatever a game can show is added here, once, and reaches every presenter.
// It is only good for the call it is given to: it points into the engine.
struct FrameView {
  const Scene& scene;
  // What the scene is made of (meshes, materials, textures), to be uploaded once per
  // `scene_generation`: the number goes up every time the game loads another scene.
  const SceneDesc& scene_desc;
  int scene_generation = 0;
  // What scripts drew over the game this frame (UI).
  // cppcheck-suppress uninitMemberVarNoCtor ; a reference: whoever makes one has to give it
  const std::vector<HudItem>& hud;
  const ParticleSystem& particles;
  float motion_blur = 0.0f;  // Camera.motion_blur: 0 (none) to 1
  // A touch screen's stick and buttons; none elsewhere.
  // cppcheck-suppress uninitMemberVarNoCtor ; a reference: whoever makes one has to give it
  const std::vector<ControlCircle>& controls;
  // Where UI.image finds its pictures.
  // cppcheck-suppress uninitMemberVarNoCtor ; a reference: whoever makes one has to give it
  const std::filesystem::path& images_dir;
  bool stop_recording = false;                         // a script ended the recording (Game.stop_recording)
  const WaterRippleSnapshot* water_ripples = nullptr;  // transient simulation state; drawing never ages it
};

// Who shows a game: a window of its own, a view inside another program, or nobody. The engine
// never opens a window or draws; it asks its presenter what it is drawn on and hands it each frame.
class Presenter {
 public:
  virtual ~Presenter() = default;
  // The size of what the game is drawn on, in pixels (what UI.width and UI.height say).
  virtual SurfaceSize surface_size() const = 0;
  // Width in pixels of a text drawn over the game at that size (UI.text_width, text alignment).
  virtual int text_width(const std::string& text, int size) const = 0;
  virtual int font_text_width(const std::string& text, int size, const std::string&,
                              const std::filesystem::path&) const {
    return text_width(text, size);
  }
  // The game asks for the pointer to be hidden and kept (Input.mouse_locked), every frame; the
  // presenter decides whether it gets it.
  virtual void capture_cursor(bool wanted) = 0;
  // Draws one frame of the game and shows it.
  virtual void present(const FrameView& view) = 0;
  // Whether there is still somewhere to show the game: false ends Engine::run (a closed window).
  virtual bool open() const = 0;
  // How long the frame before this one took: what Engine::run advances the game by.
  virtual float frame_seconds() const = 0;
  // The player's display settings (Settings in Ruby): the presenter takes what it can of them. One
  // that shows the game in no window of its own (nobody, the editor's view) takes none.
  virtual void apply_display(const DisplaySettings& /*display*/) {}
  // The sizes the player may choose for the window, smallest first; none without a window.
  virtual std::vector<SurfaceSize> display_modes() const { return {}; }
};

// Nobody watches: a game played by a test or on a server. It has the size it was given, texts
// are as wide as a usual letter makes them, and every frame counts as one fixed step.
class NullPresenter final : public Presenter {
 public:
  explicit NullPresenter(SurfaceSize size) : size_(size) {}
  SurfaceSize surface_size() const override { return size_; }
  int text_width(const std::string& text, int size) const override;
  void capture_cursor(bool) override {}
  void present(const FrameView&) override {}
  bool open() const override { return true; }
  float frame_seconds() const override;

 private:
  SurfaceSize size_;
};

}  // namespace redgem
