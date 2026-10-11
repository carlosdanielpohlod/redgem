#include "audio/audio.hpp"

#include <raylib.h>

#include <set>

#include "core/log.hpp"

namespace redgem {
namespace {

constexpr const char* kSoundExtensions[] = {".wav", ".ogg", ".mp3", ".flac"};

class RaylibAudio final : public AudioSystem {
 public:
  RaylibAudio() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) log::warning("no audio device: the game runs silently");
  }

  ~RaylibAudio() override {
    stop_music();
    for (auto& [name, sound] : sounds_) UnloadSound(sound);
    if (ready_) CloseAudioDevice();
  }

  bool play_sound(const std::string& name, float volume, float pitch) override {
    if (!ready_) return false;
    auto it = sounds_.find(name);
    if (it == sounds_.end()) {
      const auto file = resolve(name);
      if (!file) return false;
      Sound sound = LoadSound(file->string().c_str());
      if (!IsSoundValid(sound)) return false;
      it = sounds_.emplace(name, sound).first;
    }
    SetSoundVolume(it->second, volume);
    SetSoundPitch(it->second, pitch);
    PlaySound(it->second);
    return true;
  }

  bool play_music(const std::string& name, bool loop, float volume) override {
    if (!ready_) return false;
    stop_music();
    const auto file = resolve(name);
    if (!file) return false;
    music_ = LoadMusicStream(file->string().c_str());
    if (!IsMusicValid(music_)) return false;
    music_.looping = loop;
    SetMusicVolume(music_, volume);
    PlayMusicStream(music_);
    has_music_ = true;
    return true;
  }

  void stop_music() override {
    if (!has_music_) return;
    StopMusicStream(music_);
    UnloadMusicStream(music_);
    has_music_ = false;
  }

  void set_music_pitch(float pitch) override {
    if (has_music_) SetMusicPitch(music_, pitch);
  }

  void set_music_volume(float volume) override {
    if (has_music_) SetMusicVolume(music_, volume);
  }

  void set_master_volume(float volume) override {
    if (ready_) SetMasterVolume(volume);
  }

  void update() override {
    if (has_music_) UpdateMusicStream(music_);
  }

  void set_sounds_dir(const std::filesystem::path& dir) override {
    if (dir == sounds_dir_) return;
    for (auto& [name, sound] : sounds_) UnloadSound(sound);
    sounds_.clear();
    missing_.clear();
    sounds_dir_ = dir;
  }

 private:
  std::optional<std::filesystem::path> resolve(const std::string& name) {
    const auto file = find_sound_file(sounds_dir_, name);
    if (!file && missing_.insert(name).second) {
      log::warning("sound '" + name + "' not found in " + sounds_dir_.string());
    }
    return file;
  }

  bool ready_ = false;
  std::filesystem::path sounds_dir_;
  std::map<std::string, Sound> sounds_;
  std::set<std::string> missing_;
  Music music_{};
  bool has_music_ = false;
};

}  // namespace

std::optional<std::filesystem::path> find_sound_file(const std::filesystem::path& sounds_dir,
                                                     const std::string& name) {
  for (const char* extension : kSoundExtensions) {
    const std::filesystem::path candidate = sounds_dir / (name + extension);
    if (std::filesystem::exists(candidate)) return candidate;
  }
  return std::nullopt;
}

bool RecordingAudio::play_sound(const std::string& name, float, float) {
  if (!find_sound_file(sounds_dir_, name)) return false;
  played_.push_back(name);
  return true;
}

bool RecordingAudio::play_music(const std::string& name, bool, float) {
  if (!find_sound_file(sounds_dir_, name)) return false;
  music_ = name;
  return true;
}

std::unique_ptr<AudioSystem> create_raylib_audio() { return std::make_unique<RaylibAudio>(); }

}  // namespace redgem
