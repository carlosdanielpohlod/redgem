#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "audio/audio.hpp"

namespace redgem::testing {

// The audio a test listens with: no device, it resolves the files as the real one does and
// remembers what would have played. Given to an engine through EngineOptions::audio. (All of it
// is in this header, so the tests of the engine can use it without linking the testing library.)
class RecordingAudio final : public AudioSystem {
 public:
  bool play_sound(const std::string& name, float, float) override {
    if (!find_sound_file(sounds_dir_, name)) return false;
    played_.push_back(name);
    return true;
  }
  bool play_music(const std::string& name, bool, float) override {
    if (!find_sound_file(sounds_dir_, name)) return false;
    music_ = name;
    return true;
  }
  void stop_music() override { music_.reset(); }
  void set_music_pitch(float pitch) override { music_pitch_ = pitch; }
  void set_music_volume(float volume) override { music_volume_ = volume; }
  void set_master_volume(float volume) override { master_volume_ = volume; }
  void update() override {}
  void set_sounds_dir(const std::filesystem::path& dir) override { sounds_dir_ = dir; }

  const std::vector<std::string>& played() const { return played_; }
  const std::optional<std::string>& music() const { return music_; }
  float music_pitch() const { return music_pitch_; }
  float music_volume() const { return music_volume_; }
  float master_volume() const { return master_volume_; }

 private:
  std::filesystem::path sounds_dir_;
  std::vector<std::string> played_;
  std::optional<std::string> music_;
  float master_volume_ = kFullVolume;
  float music_pitch_ = kAsRecorded;
  float music_volume_ = kFullVolume;
};

}  // namespace redgem::testing
