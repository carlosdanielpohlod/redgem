#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "audio/mix_groups.hpp"
#include "audio/music_player.hpp"
#include "audio/spatial_audio.hpp"

namespace redgem {

// Sound effects and music of a game, looked up by name in its sounds/ folder
// ("jump" -> sounds/jump.wav | .ogg | .mp3 | .flac).
class AudioSystem {
 public:
  virtual ~AudioSystem() = default;
  // Every sound played gets a handle (flat or positional, one id space) that stops it, changes its
  // levels and says whether it still plays. Zero means no source: the file is missing, the backend
  // plays nothing (silent and test backends without a pool), or kMaxSpatialSounds already play.
  virtual SoundSourceId play_sound(const std::string&, const FlatSound&) { return kNoSoundSource; }
  // Positional sources overlap independently, even when they use the same asset.
  // The game supplies world positions and listener orientation; update them when objects/cameras move.
  virtual SoundSourceId play_spatial_sound(const std::string&, const SpatialSound&) { return kNoSoundSource; }
  virtual bool set_listener(const AudioListener&) { return false; }
  virtual bool move_sound(SoundSourceId, Vec3) { return false; }
  virtual bool set_sound_levels(SoundSourceId, const SoundLevels&) { return false; }
  virtual bool sound_playing(SoundSourceId) const { return false; }
  virtual bool stop_sound(SoundSourceId) { return false; }
  // Releases every sound with a handle; the music keeps playing.
  virtual void stop_sounds() {}
  // The player's volume of a group (Settings.music_volume...): what each sound of it is heard at,
  // times its own.
  virtual void set_group_volume(MixGroup, float) {}

  // A new track fades in over `request.fade_seconds` while the one playing fades out.
  virtual bool play_music(const std::string& name, const MusicRequest& request) = 0;
  virtual void fade_out_music(float seconds) = 0;
  void stop_music() { fade_out_music(kNoFade); }
  // The playing music's pitch (1 = as recorded; it also plays faster or slower) and volume. They
  // can change every frame: an engine's drone following the car's speed.
  virtual void set_music_pitch(float pitch) = 0;
  virtual void set_music_volume(float volume) = 0;
  // The game's master volume (Audio.volume). What is heard is it times the player's.
  virtual void set_master_volume(float volume) = 0;
  // The player's own volume (Settings.volume), apart from the game's: a multiplier of the master
  // volume, which it applies again.
  void set_player_volume(float volume) {
    player_volume_ = volume;
    set_master_volume(game_volume_);
  }
  // Called every frame with the frame's seconds (music streams need feeding, fades move on).
  virtual void update(float seconds) = 0;
  // Where sounds are looked up; changes with the game being played.
  virtual void set_sounds_dir(const std::filesystem::path& dir) = 0;

 protected:
  // What a master volume the game sets is heard at: it times the player's (kept, for when the
  // player's changes).
  float heard_volume(float game_volume) {
    game_volume_ = game_volume;
    return game_volume * player_volume_;
  }

 private:
  float game_volume_ = kFullVolume;
  float player_volume_ = kFullVolume;
};

// Finds the file of a sound name; nullopt when no supported file exists.
std::optional<std::filesystem::path> find_sound_file(const std::filesystem::path& sounds_dir, const std::string& name);

// No audio device (a game nobody listens to): nothing plays, and every sound is said to be missing.
class NullAudio final : public AudioSystem {
 public:
  bool play_music(const std::string&, const MusicRequest&) override { return false; }
  void fade_out_music(float) override {}
  void set_music_pitch(float) override {}
  void set_music_volume(float) override {}
  void set_master_volume(float) override {}
  void update(float) override {}
  void set_sounds_dir(const std::filesystem::path&) override {}
};

// Sounds in a SoundPool and music in a MusicPlayer, over the drivers of a backend: what every
// backend that plays something shares. A backend keeps its drivers in a base listed before this
// one, so they outlive the pool and the player, which stop through them as they go.
class MixedAudio : public AudioSystem {
 public:
  MixedAudio(SoundDriver& sounds, MusicDriver& music) : sounds_(sounds), music_(music) {}

  SoundSourceId play_sound(const std::string& name, const FlatSound& sound) override;
  SoundSourceId play_spatial_sound(const std::string& name, const SpatialSound& sound) override;
  bool set_listener(const AudioListener& listener) override;
  bool move_sound(SoundSourceId id, Vec3 position) override;
  bool set_sound_levels(SoundSourceId id, const SoundLevels& levels) override;
  bool sound_playing(SoundSourceId id) const override;
  bool stop_sound(SoundSourceId id) override;
  void stop_sounds() override;
  void set_group_volume(MixGroup group, float volume) override;
  bool play_music(const std::string& name, const MusicRequest& request) override;
  void fade_out_music(float seconds) override;
  void set_music_pitch(float pitch) override;
  void set_music_volume(float volume) override;
  void update(float seconds) override;

 protected:
  const MusicPlayer& music_player() const { return music_; }

 private:
  SoundPool sounds_;
  MusicPlayer music_;
};

// Plays through raylib. Falls back to silence when no audio device is available.
std::unique_ptr<AudioSystem> create_raylib_audio();

}  // namespace redgem
