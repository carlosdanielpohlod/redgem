#include "audio/occlusion.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

// Seconds: a shorter fade is none.
constexpr float kShortestOcclusionFade = 0.001f;

bool valid_occluded_volume(float volume) { return std::isfinite(volume) && volume >= 0.0f && volume <= kFullVolume; }
bool valid_occluded_cutoff(std::optional<float> hz) {
  if (!hz) return true;
  return std::isfinite(*hz) && *hz >= kLowestCutoff && *hz <= kHighestCutoff;
}
bool valid_occlusion_fade(float seconds) { return std::isfinite(seconds) && seconds >= 0.0f; }

// The cutoff at an occlusion, even in pitch (in octaves) from the highest to the settings' own.
float cutoff_at(float level, float occluded) { return kHighestCutoff * std::pow(occluded / kHighestCutoff, level); }

}  // namespace

bool valid_occlusion(const OcclusionSettings& settings) {
  if (!valid_occluded_volume(settings.volume)) return false;
  return valid_occluded_cutoff(settings.low_pass_hz) && valid_occlusion_fade(settings.fade_seconds);
}

bool occludes(const OcclusionSettings& settings) {
  return settings.volume < kFullVolume || settings.low_pass_hz.has_value();
}

float occlusion_step(float level, bool blocked, float seconds, const OcclusionSettings& settings) {
  const float target = blocked ? kBehindWall : kOpen;
  if (settings.fade_seconds < kShortestOcclusionFade) return target;
  const float change = std::max(seconds, 0.0f) / settings.fade_seconds;
  return target > level ? std::min(level + change, target) : std::max(level - change, target);
}

float occluded_volume(float level, const OcclusionSettings& settings) {
  return kFullVolume + (settings.volume - kFullVolume) * level;
}

VoiceEffects occluded_effects(VoiceEffects effects, float level, const OcclusionSettings& settings) {
  if (level <= kOpen) return effects;
  if (!settings.low_pass_hz) return effects;
  const float cutoff = cutoff_at(level, *settings.low_pass_hz);
  effects.low_pass_hz = std::min(effects.low_pass_hz.value_or(kHighestCutoff), cutoff);
  return effects;
}

}  // namespace redgem
