#pragma once

// How a sound plays once its import settings (core/import_settings.hpp) are taken into account.

#include "audio/spatial_audio.hpp"
#include "core/import_settings.hpp"

namespace redgem {

struct VoicePlan {
  bool stream = false;  // decoded as it plays (a Music stream) instead of loaded whole
  bool loop = false;
  float gain = kFullVolume;  // times the volume the mix gives it
  bool mono = true;          // its channels mixed down before it is panned
};

// A loop always streams (raylib loops only streams). The settings' loop makes every play loop;
// a play that asks for a loop loops anyway. Only a positional sound is ever mixed down.
VoicePlan voice_plan(const SpatialSound& sound, const import_settings::SoundSettings& settings);

}  // namespace redgem
