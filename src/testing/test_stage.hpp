#pragma once

#include <filesystem>
#include <memory>

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
  Stage(const SystemTestOptions& options, FramePictures& pictures);
  ~Stage();
  Stage(const Stage&) = delete;
  Stage& operator=(const Stage&) = delete;

  // Null when nobody shows the game.
  Presenter* presenter();
  // Where the player's display settings are kept for this run: a file of the test's own.
  const std::filesystem::path& settings_file() const { return settings_file_; }
  // Without a window there is no device either: the game still finds its sounds, and hears none.
  AudioSystem* audio() { return window_ ? speakers_.get() : &silent_; }

 private:
  std::filesystem::path settings_file_;
  std::unique_ptr<WindowPresenter> window_;
  std::unique_ptr<AudioSystem> speakers_;
  RecordingAudio silent_;
};

}  // namespace redgem::testing
