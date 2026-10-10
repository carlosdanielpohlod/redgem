#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace redgem {

// Sound effects and music of a game, looked up by name in its sounds/ folder
// ("jump" -> sounds/jump.wav | .ogg | .mp3 | .flac).
class AudioSystem {
 public:
  virtual ~AudioSystem() = default;
  virtual bool play_sound(const std::string& name, float volume, float pitch) = 0;
  virtual bool play_music(const std::string& name, bool loop, float volume) = 0;
  virtual void stop_music() = 0;
  virtual void set_master_volume(float volume) = 0;
  // Called every frame (music streams need feeding).
  virtual void update() = 0;
  // Where sounds are looked up; changes with the game being played.
  virtual void set_sounds_dir(const std::filesystem::path& dir) = 0;
};

// Finds the file of a sound name; nullopt when no supported file exists.
std::optional<std::filesystem::path> find_sound_file(const std::filesystem::path& sounds_dir, const std::string& name);

// No audio device (tests, headless runs): resolves files and records what would have played.
class RecordingAudio final : public AudioSystem {
 public:
  bool play_sound(const std::string& name, float volume, float pitch) override;
  bool play_music(const std::string& name, bool loop, float volume) override;
  void stop_music() override { music_.reset(); }
  void set_master_volume(float volume) override { master_volume_ = volume; }
  void update() override {}
  void set_sounds_dir(const std::filesystem::path& dir) override { sounds_dir_ = dir; }

  const std::vector<std::string>& played() const { return played_; }
  const std::optional<std::string>& music() const { return music_; }
  float master_volume() const { return master_volume_; }

 private:
  std::filesystem::path sounds_dir_;
  std::vector<std::string> played_;
  std::optional<std::string> music_;
  float master_volume_ = 1.0f;
};

// Plays through raylib. Falls back to silence when no audio device is available.
std::unique_ptr<AudioSystem> create_raylib_audio();

}  // namespace redgem
