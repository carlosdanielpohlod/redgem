#pragma once

#include <filesystem>
#include <memory>

#include "app/game_display.hpp"
#include "audio/audio.hpp"
#include "testing/recording_audio.hpp"
#include "testing/system_test.hpp"

namespace redgem {
class Presenter;
class WindowPresenter;
}  // namespace redgem

namespace redgem::testing {

class FramePictures;

// What a game under test is shown in and heard through: a real window with the machine's sound,
// or nothing at all (a headless run). The pictures a test asks for are taken from the window, as
// each frame is drawn.
class Stage {
 public:
  // `settings_file`: where the player's display settings are kept for the run.
  Stage(const SystemTestOptions& options, const std::filesystem::path& settings_file, FramePictures& pictures);
  ~Stage();
  Stage(const Stage&) = delete;
  Stage& operator=(const Stage&) = delete;

  // Null when nobody shows the game.
  Presenter* presenter();
  // The player's display settings the run starts with (what the settings file says over the
  // game's), and where they are kept for it.
  const DisplaySettings& display() const { return player_.display; }
  const DisplayKeeping& display_kept_in() const { return player_.kept_in; }
  // Without a window there is no device either: the game still finds its sounds, and hears none.
  AudioSystem* audio() { return window_ ? speakers_.get() : &silent_; }

 private:
  PlayerDisplay player_;
  std::unique_ptr<WindowPresenter> window_;
  std::unique_ptr<AudioSystem> speakers_;
  RecordingAudio silent_;
};

}  // namespace redgem::testing
