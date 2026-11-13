// The audio backend that plays through raylib's device.
//
// Raylib has two kinds of source, and each sound takes the one that suits it:
//   - a Sound is decoded once into memory and starts at once; LoadSoundAlias gives each play a
//     voice of its own (volume, pitch, pan, end) over the same samples, so a sound played often
//     (footsteps, shots) is never decoded again and its plays overlap. Raylib cannot loop a Sound;
//   - a Music stream decodes as it plays, fed every frame, and loops without a gap. Loops (an
//     engine's hum, an ambience) and the music are streams; a crossfade is two of them.

#include <raylib.h>

#include <map>
#include <memory>
#include <set>
#include <variant>

#include "audio/audio.hpp"
#include "audio/game_music.hpp"
#include "core/log.hpp"

namespace redgem {
namespace {

// The game's sounds folder, and the Sounds of it decoded so far (each name decoded once).
class SoundLibrary {
 public:
  ~SoundLibrary() { forget(); }
  void open(bool ready) { ready_ = ready; }
  bool ready() const { return ready_; }
  const std::filesystem::path& dir() const { return dir_; }
  void set_dir(const std::filesystem::path& dir) {
    forget();
    missing_.clear();
    dir_ = dir;
  }
  // The file of a name, said once for each missing name (a game asks for a sound every time).
  std::optional<std::filesystem::path> file(const std::string& name) {
    auto found = find_sound_file(dir_, name);
    if (!found) say_missing(name);
    return found;
  }
  // The decoded samples of a name; nullptr when it has no file that decodes.
  const Sound* sound(const std::string& name) {
    const auto known = sounds_.find(name);
    if (known != sounds_.end()) return &known->second;
    const auto found = file(name);
    if (!found) return nullptr;
    const Sound loaded = LoadSound(found->string().c_str());
    if (!IsSoundValid(loaded)) return nullptr;
    return &sounds_.emplace(name, loaded).first->second;
  }
  void forget() {
    for (auto& [name, sound] : sounds_) UnloadSound(sound);
    sounds_.clear();
  }

 private:
  void say_missing(const std::string& name) {
    if (missing_.insert(name).second) log::warning("sound '" + name + "' not found in " + dir_.string());
  }

  bool ready_ = false;
  std::filesystem::path dir_;
  std::map<std::string, Sound> sounds_;
  std::set<std::string> missing_;
};

std::optional<Music> load_music(const std::filesystem::path& file) {
  Music music = load_game_music(file);
  const bool valid = IsMusicValid(music) && IsAudioStreamValid(music.stream);
  if (valid) return music;
  unload_game_music(music);
  return std::nullopt;
}

// A voice of a sound: an alias of its decoded samples, or a stream for a loop.
using Voice = std::variant<Sound, Music>;
AudioStream voice_stream(const Voice& voice) {
  return std::visit([](const auto& source) { return source.stream; }, voice);
}
void unload_voice(Sound alias) { UnloadSoundAlias(alias); }
void unload_voice(Music music) { unload_game_music(music); }

std::optional<Voice> load_alias(SoundLibrary& library, const std::string& name) {
  const Sound* source = library.sound(name);
  if (source == nullptr) return std::nullopt;
  const Sound alias = LoadSoundAlias(*source);
  if (IsSoundValid(alias)) return Voice{alias};
  UnloadSoundAlias(alias);
  return std::nullopt;
}
std::optional<Voice> load_loop(SoundLibrary& library, const std::string& name) {
  const auto file = library.file(name);
  if (!file) return std::nullopt;
  const auto music = load_music(*file);
  if (!music) return std::nullopt;
  return Voice{*music};
}
std::optional<Voice> load_voice(SoundLibrary& library, const std::string& name, bool loop) {
  return loop ? load_loop(library, name) : load_alias(library, name);
}

void spatial_mono_callback(void* buffer, unsigned frames) { spatial_mono_frames(static_cast<float*>(buffer), frames); }
void apply_mix(AudioStream stream, SpatialMix mix) {
  constexpr float kPanCenter = 0.5f;  // raylib uses 0 for right and 1 for left
  SetAudioStreamVolume(stream, mix.volume);
  SetAudioStreamPan(stream, kPanCenter * (1.0f - mix.pan));
}
// A positional sound is panned from mono, so the source's own channels do not pull it aside; a
// flat one keeps its stereo.
void prepare_stream(AudioStream stream, const SpatialSound& sound, SpatialMix mix) {
  if (sound.positional) AttachAudioStreamProcessor(stream, spatial_mono_callback);
  SetAudioStreamPitch(stream, sound.pitch);
  apply_mix(stream, mix);
}

class RaylibSoundDriver final : public SoundDriver {
 public:
  explicit RaylibSoundDriver(SoundLibrary& library) : library_(library) {}
  bool start(SoundSourceId id, const std::string& name, const SpatialSound& sound, SpatialMix mix) override {
    if (!library_.ready()) return false;
    auto voice = load_voice(library_, name, sound.loop);
    if (!voice) return false;
    const auto stream = voice_stream(*voice);
    prepare_stream(stream, sound, mix);
    voices_.emplace(id, *voice);
    PlayAudioStream(stream);
    return true;
  }
  void update(SoundSourceId id) override {
    if (auto* music = std::get_if<Music>(&voices_.at(id))) UpdateMusicStream(*music);
  }
  bool playing(SoundSourceId id) const override { return IsAudioStreamPlaying(voice_stream(voices_.at(id))); }
  void mix(SoundSourceId id, SpatialMix mix) override { apply_mix(voice_stream(voices_.at(id)), mix); }
  void pitch(SoundSourceId id, float pitch) override { SetAudioStreamPitch(voice_stream(voices_.at(id)), pitch); }
  void stop(SoundSourceId id) override {
    const auto found = voices_.find(id);
    if (found == voices_.end()) return;
    const auto stream = voice_stream(found->second);
    StopAudioStream(stream);
    DetachAudioStreamProcessor(stream, spatial_mono_callback);  // (nothing to detach from a flat one)
    std::visit([](const auto& voice) { unload_voice(voice); }, found->second);
    voices_.erase(found);
  }

 private:
  SoundLibrary& library_;
  std::map<SoundSourceId, Voice> voices_;
};

class RaylibMusicDriver final : public MusicDriver {
 public:
  explicit RaylibMusicDriver(SoundLibrary& library) : library_(library) {}
  bool start(MusicTrackId id, const std::string& name, bool loop, float volume) override {
    if (!library_.ready()) return false;
    const auto file = library_.file(name);
    if (!file) return false;
    auto music = load_music(*file);
    if (!music) return false;
    music->looping = loop;
    SetMusicVolume(*music, volume);
    PlayMusicStream(*music);
    tracks_.emplace(id, *music);
    return true;
  }
  void update(MusicTrackId id) override { UpdateMusicStream(tracks_.at(id)); }
  void volume(MusicTrackId id, float volume) override { SetMusicVolume(tracks_.at(id), volume); }
  void pitch(MusicTrackId id, float pitch) override { SetMusicPitch(tracks_.at(id), pitch); }
  void stop(MusicTrackId id) override {
    const auto found = tracks_.find(id);
    if (found == tracks_.end()) return;
    StopMusicStream(found->second);
    unload_game_music(found->second);
    tracks_.erase(found);
  }

 private:
  SoundLibrary& library_;
  std::map<MusicTrackId, Music> tracks_;
};

// The device, opened first and closed last.
class RaylibDevice {
 public:
  RaylibDevice() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) log::warning("no audio device: the game runs silently");
  }
  ~RaylibDevice() {
    if (ready_) CloseAudioDevice();
  }
  RaylibDevice(const RaylibDevice&) = delete;
  RaylibDevice& operator=(const RaylibDevice&) = delete;
  bool ready() const { return ready_; }

 private:
  bool ready_ = false;
};

// What the pool and the music player play through, made before them and gone after them (in
// reverse: the drivers, the decoded sounds, then the device).
struct RaylibParts {
  RaylibDevice device;
  SoundLibrary library;
  RaylibSoundDriver sound_driver{library};
  RaylibMusicDriver music_driver{library};
};

class RaylibAudio final : private RaylibParts, public MixedAudio {
 public:
  RaylibAudio() : MixedAudio(sound_driver, music_driver) { library.open(device.ready()); }

  void set_master_volume(float volume) override {
    const float heard = heard_volume(volume);
    if (device.ready()) SetMasterVolume(heard);
  }

  void set_sounds_dir(const std::filesystem::path& dir) override {
    if (dir == library.dir()) return;
    stop_sounds();  // (their aliases share the samples about to go)
    library.set_dir(dir);
  }
};

}  // namespace

std::unique_ptr<AudioSystem> create_raylib_audio() { return std::make_unique<RaylibAudio>(); }

}  // namespace redgem
