#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "audio/audio.hpp"

namespace redgem::testing {

// A sound the recording audio would be playing, as it was last mixed.
struct RecordedVoice {
  std::string name;
  SpatialSound sound;
  SpatialMix mix;
  bool playing = true;  // until RecordingAudio::finish says it came to its end
};

// A music stream it would be playing, at the volume it would be heard at.
struct RecordedTrack {
  std::string name;
  bool loop = true;
  float volume = kFullVolume;
  float pitch = kAsRecorded;
};

// Sounds whose file is there, kept instead of played.
class RecordingSoundDriver final : public SoundDriver {
 public:
  explicit RecordingSoundDriver(const std::filesystem::path& dir) : dir_(dir) {}
  bool start(SoundSourceId id, const std::string& name, const SpatialSound& sound, SpatialMix mix) override {
    if (!find_sound_file(dir_, name)) return false;
    voices.emplace(id, RecordedVoice{name, sound, mix});
    started.push_back(name);
    return true;
  }
  void update(SoundSourceId) override {}
  bool playing(SoundSourceId id) const override { return voices.at(id).playing; }
  void mix(SoundSourceId id, SpatialMix mix) override { voices.at(id).mix = mix; }
  void pitch(SoundSourceId id, float pitch) override { voices.at(id).sound.pitch = pitch; }
  void stop(SoundSourceId id) override { voices.erase(id); }

  std::map<SoundSourceId, RecordedVoice> voices;
  std::vector<std::string> started;  // every name that started, in order

 private:
  const std::filesystem::path& dir_;
};

class RecordingMusicDriver final : public MusicDriver {
 public:
  explicit RecordingMusicDriver(const std::filesystem::path& dir) : dir_(dir) {}
  bool start(MusicTrackId id, const std::string& name, bool loop, float volume) override {
    if (!find_sound_file(dir_, name)) return false;
    tracks.emplace(id, RecordedTrack{name, loop, volume, kAsRecorded});
    return true;
  }
  void update(MusicTrackId) override {}
  void volume(MusicTrackId id, float volume) override { tracks.at(id).volume = volume; }
  void pitch(MusicTrackId id, float pitch) override { tracks.at(id).pitch = pitch; }
  void stop(MusicTrackId id) override { tracks.erase(id); }

  std::map<MusicTrackId, RecordedTrack> tracks;

 private:
  const std::filesystem::path& dir_;
};

// What the recording audio keeps, made before it and gone after it.
struct RecordingParts {
  std::filesystem::path sounds_dir;
  RecordingSoundDriver sound_driver{sounds_dir};
  RecordingMusicDriver music_driver{sounds_dir};
};

// The audio a test listens with: no device, it resolves the files as the real one does and
// remembers what would have played, through the same pool and music player as the real one.
// Given to an engine through EngineOptions::audio. (All of it is in this header, so the tests of
// the engine can use it without linking the testing library.)
class RecordingAudio final : private RecordingParts, public MixedAudio {
 public:
  RecordingAudio() : MixedAudio(sound_driver, music_driver) {}

  void set_music_pitch(float pitch) override {
    music_pitch_ = pitch;
    MixedAudio::set_music_pitch(pitch);
  }
  void set_music_volume(float volume) override {
    music_volume_ = volume;
    MixedAudio::set_music_volume(volume);
  }
  void set_master_volume(float volume) override {
    master_volume_ = volume;
    heard_master_volume_ = heard_volume(volume);
  }
  void set_group_volume(MixGroup group, float volume) override {
    group_volumes_[mix_group_index(group)] = volume;
    MixedAudio::set_group_volume(group, volume);
  }
  void set_sounds_dir(const std::filesystem::path& dir) override {
    stop_sounds();
    sounds_dir = dir;
  }
  // The end of a sound that does not loop.
  void finish(SoundSourceId id) { sound_driver.voices.at(id).playing = false; }

  const std::vector<std::string>& played() const { return sound_driver.started; }
  const std::map<SoundSourceId, RecordedVoice>& voices() const { return sound_driver.voices; }
  // Every music stream playing, the one fading out included.
  const std::map<MusicTrackId, RecordedTrack>& tracks() const { return music_driver.tracks; }
  // The name of the track playing (not one fading out).
  std::optional<std::string> music() const {
    const auto playing = music_player().current();
    if (!playing) return std::nullopt;
    return music_driver.tracks.at(*playing).name;
  }
  float music_pitch() const { return music_pitch_; }                  // as the game last set it
  float music_volume() const { return music_volume_; }                // as the game last set it
  float master_volume() const { return master_volume_; }              // the game's
  float heard_master_volume() const { return heard_master_volume_; }  // the game's times the player's
  float group_volume(MixGroup group) const { return group_volumes_[mix_group_index(group)]; }

 private:
  float master_volume_ = kFullVolume;
  float heard_master_volume_ = kFullVolume;
  float music_pitch_ = kAsRecorded;
  float music_volume_ = kFullVolume;
  GroupVolumes group_volumes_ = kFullGroupVolumes;
};

}  // namespace redgem::testing
