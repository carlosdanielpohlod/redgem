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

std::optional<MixGroup> MixedAudio::define_group(const std::string& name, MixGroup parent, float volume) {
  const auto group = AudioSystem::define_group(name, parent, volume);
  sounds_.refresh();  // (a group that moved is heard under its new parent, with its effects)
  sounds_.refresh_effects();
  return group;
}

bool MixedAudio::set_group_effects(MixGroup group, const GroupEffects& effects) {
  if (!AudioSystem::set_group_effects(group, effects)) return false;
  sounds_.refresh_effects();
  return true;
}

void MixedAudio::forget_game_groups() {
  AudioSystem::forget_game_groups();
  sounds_.refresh();
  sounds_.refresh_effects();
}

bool MixedAudio::set_music_ducking(const MusicDucking& ducking) { return ducker_.set(ducking); }

bool MixedAudio::play_music(const std::string& name, const MusicRequest& request) { return music_.play(name, request); }
void MixedAudio::fade_out_music(float seconds) { music_.stop(seconds); }
void MixedAudio::set_music_pitch(float pitch) { music_.set_pitch(pitch); }
void MixedAudio::set_music_volume(float volume) { music_.set_volume(volume); }

void MixedAudio::update(float seconds) {
  sounds_.update();  // (a voice that ended no longer ducks)
  duck(seconds);
  music_.update(seconds);
}

// The music group, its sounds and its tracks, follows the ducker.
void MixedAudio::duck(float seconds) {
  const float gain = ducker_.update(sounds_.group_playing(MixGroup::Voice), seconds);
  sounds_.set_group_gain(MixGroup::Music, gain);
  music_.set_ducked(gain);
}

}  // namespace redgem
