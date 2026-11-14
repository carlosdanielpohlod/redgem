#pragma once

// The effects of the sounds playing, where the audio thread runs them. Each sound playing has a
// slot: its group's low-pass, and what it sends to its group's reverb bus. After every sound of a
// device period went through its slot, the buses' reverbs are added to the mix. The game's thread
// sets the slots; the audio thread reads them (one lock, held for a slot or a mix at a time).
// Nothing in it needs a device: the tests run it on buffers of their own.

#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "audio/audio_effects.hpp"
#include "audio/spatial_audio.hpp"

namespace redgem {

// One more than the sounds at once: a sound that takes another's place starts before that one stops.
inline constexpr std::size_t kEffectSlots = kMaxSounds + 1;
// The frames of one device period a bus holds (about 170 ms at 48 kHz); a longer period's send
// is cut there.
inline constexpr std::size_t kMostBusFrames = 8192;
using EffectSlot = std::size_t;

class EffectRack {
 public:
  EffectRack();
  // The game's thread.
  std::optional<EffectSlot> claim();  // a free slot, with no effects
  void release(EffectSlot slot);
  void set_effects(EffectSlot slot, const VoiceEffects& effects);
  void set_volume(EffectSlot slot, float volume);  // what the sound is heard at, for its send
  // The audio thread: a sound's frames (interleaved stereo, before its volume and pan), then the
  // mix of every sound.
  void process_voice(EffectSlot slot, float* frames, std::size_t count);
  void process_mix(float* frames, std::size_t count);

 private:
  struct Slot {
    bool used = false;
    LowPass filter;
    std::optional<float> cutoff;
    float reverb = kNoReverb;
    std::size_t bus = 0;
    float volume = kFullVolume;
    std::size_t sent = 0;  // frames it sent this period
  };
  struct Bus {
    std::unique_ptr<Reverb> reverb;  // made the first time a sound sends to it
    std::vector<float> send;
  };
  void send(Slot& slot, const float* frames, std::size_t count);
  void open_bus(std::size_t bus);

  std::mutex mutex_;
  std::array<Slot, kEffectSlots> slots_;
  std::array<Bus, kMaxMixGroups> buses_;
};

}  // namespace redgem
