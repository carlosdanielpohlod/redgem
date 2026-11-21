#include "audio/sound_import.hpp"

namespace redgem {

VoicePlan voice_plan(const SpatialSound& sound, const import_settings::SoundSettings& settings) {
  VoicePlan plan;
  plan.loop = sound.loop || settings.loop;
  plan.stream = plan.loop || settings.stream;
  plan.gain = settings.volume;
  plan.mono = sound.positional && settings.mono_3d;
  return plan;
}

}  // namespace redgem
