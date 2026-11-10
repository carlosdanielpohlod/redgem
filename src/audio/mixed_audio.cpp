#include "audio/audio.hpp"

namespace redgem {

SoundSourceId MixedAudio::play_sound(const std::string& name, const FlatSound& sound) {
  return sounds_.play(name, flat_sound(sound));
}

SoundSourceId MixedAudio::play_spatial_sound(const std::string& name, const SpatialSound& sound) {
  return sounds_.play(name, sound);
}

bool MixedAudio::set_listener(const AudioListener& listener) { return sounds_.set_listener(listener); }
bool MixedAudio::move_sound(SoundSourceId id, Vec3 position) { return sounds_.move(id, position); }
bool MixedAudio::set_sound_levels(SoundSourceId id, const SoundLevels& levels) {
  return sounds_.set_levels(id, levels);
}
bool MixedAudio::sound_playing(SoundSourceId id) const { return sounds_.playing(id); }
bool MixedAudio::stop_sound(SoundSourceId id) { return sounds_.stop(id); }
void MixedAudio::stop_sounds() { sounds_.clear(); }

void MixedAudio::set_group_volume(MixGroup group, float volume) {
  sounds_.set_group_volume(group, volume);
  if (group == MixGroup::Music) music_.set_group_volume(volume);
}

bool MixedAudio::play_music(const std::string& name, const MusicRequest& request) { return music_.play(name, request); }
void MixedAudio::fade_out_music(float seconds) { music_.stop(seconds); }
void MixedAudio::set_music_pitch(float pitch) { music_.set_pitch(pitch); }
void MixedAudio::set_music_volume(float volume) { music_.set_volume(volume); }

void MixedAudio::update(float seconds) {
  sounds_.update();
  music_.update(seconds);
}

}  // namespace redgem
