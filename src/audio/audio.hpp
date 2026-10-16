#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace redgem {

// What the audio is at before a game changes it: every sound and music at its full volume, the
// music at the pitch it was recorded at.
inline constexpr float kFullVolume = 1.0f;
inline constexpr float kAsRecorded = 1.0f;

// Sound effects and music of a game, looked up by name in its sounds/ folder
// ("jump" -> sounds/jump.wav | .ogg | .mp3 | .flac).
class AudioSystem {
 public:
  virtual ~AudioSystem() = default;
  virtual bool play_sound(const std::string& name, float volume, float pitch) = 0;
  virtual bool play_music(const std::string& name, bool loop, float volume) = 0;
  virtual void stop_music() = 0;
  // The playing music's pitch (1 = as recorded; it also plays faster or slower) and volume. They
  // can change every frame: an engine's drone following the car's speed.
  virtual void set_music_pitch(float pitch) = 0;
  virtual void set_music_volume(float volume) = 0;
  virtual void set_master_volume(float volume) = 0;
  // Called every frame (music streams need feeding).
  virtual void update() = 0;
  // Where sounds are looked up; changes with the game being played.
  virtual void set_sounds_dir(const std::filesystem::path& dir) = 0;
};

// Finds the file of a sound name; nullopt when no supported file exists.
std::optional<std::filesystem::path> find_sound_file(const std::filesystem::path& sounds_dir, const std::string& name);

// No audio device (a game nobody listens to): nothing plays, and every sound is said to be missing.
class NullAudio final : public AudioSystem {
 public:
  bool play_sound(const std::string&, float, float) override { return false; }
  bool play_music(const std::string&, bool, float) override { return false; }
  void stop_music() override {}
  void set_music_pitch(float) override {}
  void set_music_volume(float) override {}
  void set_master_volume(float) override {}
  void update() override {}
  void set_sounds_dir(const std::filesystem::path&) override {}
};

// Plays through raylib. Falls back to silence when no audio device is available.
std::unique_ptr<AudioSystem> create_raylib_audio();

}  // namespace redgem
