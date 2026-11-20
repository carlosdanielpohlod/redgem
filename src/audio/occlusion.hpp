#pragma once

// A positional sound behind a wall is heard quieter and muffled. Every frame the pool asks the
// world whether something stands between the listener and each sound (the engine answers with a
// ray through the level's static bodies), and each sound's occlusion moves toward what it found
// over a short fade, so walking past a door does not click.

#include <functional>
#include <optional>

#include "audio/audio_effects.hpp"
#include "core/math.hpp"

namespace redgem {

inline constexpr float kDefaultOccludedVolume = 0.4f;
inline constexpr float kDefaultOccludedCutoff = 1000.0f;  // Hz
inline constexpr float kDefaultOcclusionFade = 0.2f;      // seconds from open to behind a wall
inline constexpr float kOpen = 0.0f;                      // an occlusion: nothing in the way
inline constexpr float kBehindWall = 1.0f;

// How a sound fully behind a wall is heard (Audio.occlusion). A volume of 1 and no cutoff is no
// occlusion.
struct OcclusionSettings {
  float volume = kDefaultOccludedVolume;
  std::optional<float> low_pass_hz = kDefaultOccludedCutoff;
  float fade_seconds = kDefaultOcclusionFade;
};
bool valid_occlusion(const OcclusionSettings& settings);
// Whether the settings change how anything is heard (a sound need not be looked at otherwise).
bool occludes(const OcclusionSettings& settings);

// Whether something stands between two points: nullopt when that cannot be told now without
// waiting (the world is in the middle of a step), and the pool asks again next frame.
using OcclusionQuery = std::function<std::optional<bool>(Vec3 from, Vec3 to)>;

// A sound's occlusion `seconds` later, from kOpen to kBehindWall, moving toward what was found.
float occlusion_step(float level, bool blocked, float seconds, const OcclusionSettings& settings);
// What the sound's volume is multiplied by at that occlusion.
float occluded_volume(float level, const OcclusionSettings& settings);
// A sound's effects with its occlusion's low-pass: the lower of its group's cutoff and the
// occlusion's, which goes from kHighestCutoff when open to the settings' behind a wall.
VoiceEffects occluded_effects(VoiceEffects effects, float level, const OcclusionSettings& settings);

}  // namespace redgem
