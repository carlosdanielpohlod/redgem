#include "audio/effect_rack.hpp"

#include <algorithm>

namespace redgem {
namespace {

using Lock = std::lock_guard<std::mutex>;

bool sends(float reverb) { return reverb > kNoReverb; }

}  // namespace

EffectRack::EffectRack() = default;

std::optional<EffectSlot> EffectRack::claim() {
  const Lock lock(mutex_);
  for (EffectSlot slot = 0; slot < kEffectSlots; ++slot) {
    if (slots_[slot].used) continue;
    slots_[slot] = Slot{};
    slots_[slot].used = true;
    return slot;
  }
  return std::nullopt;
}

void EffectRack::release(EffectSlot slot) {
  const Lock lock(mutex_);
  slots_.at(slot).used = false;
}

void EffectRack::set_effects(EffectSlot slot, const VoiceEffects& effects) {
  const Lock lock(mutex_);
  Slot& own = slots_.at(slot);
  if (!own.cutoff) own.filter.reset();  // (a filter off kept its old level)
  if (own.cutoff != effects.low_pass_hz) own.filter.set_cutoff(effects.low_pass_hz);
  own.cutoff = effects.low_pass_hz;
  own.reverb = effects.reverb;
  own.bus = mix_group_index(effects.reverb_bus);
  if (sends(own.reverb)) open_bus(own.bus);
}

void EffectRack::set_volume(EffectSlot slot, float volume) {
  const Lock lock(mutex_);
  slots_.at(slot).volume = volume;
}

void EffectRack::close_buses() {
  const Lock lock(mutex_);
  for (Bus& bus : buses_) bus = Bus{};
}

// (Under the lock.)
void EffectRack::open_bus(std::size_t bus) {
  Bus& own = buses_.at(bus);
  if (own.reverb) return;
  own.reverb = std::make_unique<Reverb>();
  own.send.assign(kMostBusFrames * kEffectChannels, 0.0f);
}

void EffectRack::process_voice(EffectSlot slot, float* frames, std::size_t count) {
  const Lock lock(mutex_);
  Slot& own = slots_.at(slot);
  own.filter.process(frames, count);
  if (sends(own.reverb)) send(own, frames, count);
  own.sent += count;
}

// The sound's frames into its bus, where this period's sends are, at its volume.
void EffectRack::send(Slot& slot, const float* frames, std::size_t count) {
  Bus& bus = buses_[slot.bus];
  if (slot.sent >= kMostBusFrames) return;
  const std::size_t room = kMostBusFrames - slot.sent;
  const std::size_t samples = std::min(count, room) * kEffectChannels;
  const float level = slot.reverb * slot.volume;
  float* into = bus.send.data() + slot.sent * kEffectChannels;
  for (std::size_t sample = 0; sample < samples; ++sample) into[sample] += frames[sample] * level;
  bus.written = std::max(bus.written, slot.sent + samples / kEffectChannels);
}

void EffectRack::process_mix(float* frames, std::size_t count) {
  const Lock lock(mutex_);
  const std::size_t heard = std::min(count, kMostBusFrames);
  for (Bus& bus : buses_) {
    if (!bus.reverb) continue;
    bus.reverb->process(bus.send.data(), frames, heard);
    std::fill_n(bus.send.begin(), std::max(heard, bus.written) * kEffectChannels, 0.0f);
    bus.written = 0;
  }
  for (Slot& slot : slots_) slot.sent = 0;
}

}  // namespace redgem
