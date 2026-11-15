// The audio backend that plays through raylib's device.
//
// Raylib has two kinds of source, and each sound takes the one that suits it:
//   - a Sound is decoded once into memory and starts at once; LoadSoundAlias gives each play a
//     voice of its own (volume, pitch, pan, end) over the same samples, so a sound played often
//     (footsteps, shots) is never decoded again and its plays overlap. Raylib cannot loop a Sound;
//   - a Music stream decodes as it plays, fed every frame, and loops without a gap. Loops (an
//     engine's hum, an ambience) and the music are streams; a crossfade is two of them.
//
// Each sound goes through a slot of the effect rack (its group's low-pass and reverb send), and
// the buses' reverbs go into the device's mix after every sound of a period. Raylib's processors
// take no context, so each slot has a function of its own.

#include <raylib.h>

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <variant>

#include "audio/audio.hpp"
#include "audio/effect_rack.hpp"
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

// One rack for the device, which is one for the process.
EffectRack& effect_rack() {
  static EffectRack rack;
  return rack;
}
template <EffectSlot Slot>
void slot_callback(void* buffer, unsigned frames) {
  effect_rack().process_voice(Slot, static_cast<float*>(buffer), frames);
}
template <EffectSlot... Slots>
constexpr std::array<AudioCallback, sizeof...(Slots)> slot_callbacks(std::index_sequence<Slots...> /*slots*/) {
  return {&slot_callback<Slots>...};
}
constexpr auto kSlotCallbacks = slot_callbacks(std::make_index_sequence<kEffectSlots>{});
void mix_callback(void* buffer, unsigned frames) { effect_rack().process_mix(static_cast<float*>(buffer), frames); }

// A sound playing: its voice, and its slot of the rack (none when every slot is taken: it plays
// with no effects).
struct PlayingVoice {
  Voice voice;
  std::optional<EffectSlot> slot;
  float send_volume;  // the last volume the rack was told, so a mix that keeps it skips the rack's lock
};
void send_volume(PlayingVoice& playing, float volume) {
  if (!playing.slot || playing.send_volume == volume) return;
  playing.send_volume = volume;
  effect_rack().set_volume(*playing.slot, volume);
}
std::optional<EffectSlot> attach_slot(AudioStream stream) {
  const auto slot = effect_rack().claim();
  if (slot) AttachAudioStreamProcessor(stream, kSlotCallbacks.at(*slot));
  return slot;
}
// (Detached first: the audio thread no longer runs the slot once it is free again.)
void detach_slot(AudioStream stream, std::optional<EffectSlot> slot) {
  if (!slot) return;
  DetachAudioStreamProcessor(stream, kSlotCallbacks.at(*slot));
  effect_rack().release(*slot);
}
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

// The meters of the voices that play: raylib calls a stream's processor with no word of which
// stream it is, so each meter has a callback of its own, and a voice takes a free one as it starts.
// Past kVoiceMeters voices at once, a new one waits for a meter to be free (its loudness stays 0
// until then).
constexpr std::size_t kVoiceMeters = 8;
std::array<LevelMeter, kVoiceMeters> voice_meters;

template <std::size_t kMeter>
void measure_voice(void* buffer, unsigned frames) {
  voice_meters[kMeter].measure(static_cast<const float*>(buffer), static_cast<std::size_t>(frames) * kDeviceChannels);
}
template <std::size_t... kMeters>
constexpr std::array<AudioCallback, kVoiceMeters> meter_callbacks(std::index_sequence<kMeters...>) {
  return {measure_voice<kMeters>...};
}
constexpr std::array<AudioCallback, kVoiceMeters> kMeterCallbacks =
    meter_callbacks(std::make_index_sequence<kVoiceMeters>{});

// Which voice has which meter.
class VoiceMeters {
 public:
  // False when every meter is taken: the voice can ask again.
  bool attach(SoundSourceId id, AudioStream stream) {
    const auto meter = free_meter();
    if (!meter) return false;
    voice_meters[*meter].reset();
    taken_[*meter] = id;
    AttachAudioStreamProcessor(stream, kMeterCallbacks[*meter]);
    return true;
  }
  void detach(SoundSourceId id, AudioStream stream) {
    const auto meter = meter_of(id);
    if (!meter) return;
    DetachAudioStreamProcessor(stream, kMeterCallbacks[*meter]);
    taken_[*meter] = kNoSoundSource;
  }
  float level(SoundSourceId id) const {
    const auto meter = meter_of(id);
    return meter ? voice_meters[*meter].level() : kSilence;
  }

 private:
  std::optional<std::size_t> free_meter() const { return meter_of(kNoSoundSource); }
  std::optional<std::size_t> meter_of(SoundSourceId id) const {
    for (std::size_t meter = 0; meter < kVoiceMeters; ++meter) {
      if (taken_[meter] == id) return meter;
    }
    return std::nullopt;
  }

  std::array<SoundSourceId, kVoiceMeters> taken_{};  // kNoSoundSource: free
};

class RaylibSoundDriver final : public SoundDriver {
 public:
  explicit RaylibSoundDriver(SoundLibrary& library) : library_(library) {}
  bool start(SoundSourceId id, const std::string& name, const SpatialSound& sound, SpatialMix mix,
             const VoiceEffects& effects) override {
    if (!library_.ready()) return false;
    auto voice = load_voice(library_, name, sound.loop);
    if (!voice) return false;
    const auto stream = voice_stream(*voice);
    prepare_stream(stream, sound, mix);
    const auto slot = attach_slot(stream);
    if (slot) effect_rack().set_effects(*slot, effects);
    if (slot) effect_rack().set_volume(*slot, mix.volume);
    voices_.emplace(id, PlayingVoice{*voice, slot, mix.volume});
    PlayAudioStream(stream);
    return true;
  }
  void update(SoundSourceId id) override {
    meter(id, stream_of(id));
    if (auto* music = std::get_if<Music>(&voices_.at(id).voice)) UpdateMusicStream(*music);
  }
  bool playing(SoundSourceId id) const override { return IsAudioStreamPlaying(stream_of(id)); }
  void mix(SoundSourceId id, SpatialMix mix) override {
    apply_mix(stream_of(id), mix);
    send_volume(voices_.at(id), mix.volume);
  }
  void pitch(SoundSourceId id, float pitch) override { SetAudioStreamPitch(stream_of(id), pitch); }
  void effects(SoundSourceId id, const VoiceEffects& effects) override {
    const auto slot = voices_.at(id).slot;
    if (slot) effect_rack().set_effects(*slot, effects);
  }
  void close_reverbs() override { effect_rack().close_buses(); }
  void measure(SoundSourceId id) override {
    unmetered_.insert(id);
    meter(id, stream_of(id));
  }
  float level(SoundSourceId id) const override { return meters_.level(id); }
  void stop(SoundSourceId id) override {
    const auto found = voices_.find(id);
    if (found == voices_.end()) return;
    const auto stream = voice_stream(found->second.voice);
    StopAudioStream(stream);
    DetachAudioStreamProcessor(stream, spatial_mono_callback);  // (nothing to detach from a flat one)
    meters_.detach(id, stream);
    unmetered_.erase(id);
    detach_slot(stream, found->second.slot);
    std::visit([](const auto& voice) { unload_voice(voice); }, found->second.voice);
    voices_.erase(found);
  }

 private:
  AudioStream stream_of(SoundSourceId id) const { return voice_stream(voices_.at(id).voice); }
  // A voice being measured takes a meter as it starts, or as soon as one is free.
  void meter(SoundSourceId id, AudioStream stream) {
    if (unmetered_.count(id) != 0 && meters_.attach(id, stream)) unmetered_.erase(id);
  }

  SoundLibrary& library_;
  std::map<SoundSourceId, PlayingVoice> voices_;
  VoiceMeters meters_;
  std::set<SoundSourceId> unmetered_;  // voices waiting for a meter
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
    if (ready_) AttachAudioMixedProcessor(mix_callback);
  }
  ~RaylibDevice() {
    if (!ready_) return;
    DetachAudioMixedProcessor(mix_callback);
    CloseAudioDevice();
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
