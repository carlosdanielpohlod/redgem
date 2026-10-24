#include "testing/test_stage.hpp"

#include "app/window_presenter.hpp"
#include "testing/frame_pictures.hpp"

namespace redgem::testing {

Stage::Stage(const SystemTestOptions& options, const std::filesystem::path& settings_file, FramePictures& pictures)
    : player_(player_display(options.game_dir, settings_file)) {
  if (options.headless) return;
  GameWindowOptions window;
  window.game_dir = options.game_dir;
  window.gpu = options.gpu;
  window.display = player_.display;
  window.capture_cursor = false;  // the pointer stays with whoever sits at the machine
  window.on_frame_drawn = [&pictures] { pictures.save_pending(); };
  window_ = std::make_unique<WindowPresenter>(window);
  speakers_ = create_raylib_audio();
}

Stage::~Stage() = default;

const DisplaySettings& Stage::display() const { return window_ ? window_->shown() : player_.display; }

Presenter* Stage::presenter() { return window_.get(); }

}  // namespace redgem::testing
