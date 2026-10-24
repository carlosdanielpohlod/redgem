#include "testing/test_stage.hpp"

#include "app/window_presenter.hpp"
#include "testing/frame_pictures.hpp"
#include "testing/test_saves.hpp"

namespace redgem::testing {

Stage::Stage(const SystemTestOptions& options, FramePictures& pictures)
    : settings_file_(fresh_settings_file_for(options.work_dir, options.test_file)) {
  if (options.headless) return;
  GameWindowOptions window;
  window.settings_file = settings_file_;
  window.game_dir = options.game_dir;
  window.gpu = options.gpu;
  window.capture_cursor = false;  // the pointer stays with whoever sits at the machine
  window.on_frame_drawn = [&pictures] { pictures.save_pending(); };
  window_ = std::make_unique<WindowPresenter>(window);
  speakers_ = create_raylib_audio();
}

Stage::~Stage() = default;

Presenter* Stage::presenter() { return window_.get(); }

}  // namespace redgem::testing
