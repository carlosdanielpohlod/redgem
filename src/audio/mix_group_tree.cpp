#include "audio/mix_group_tree.hpp"

#include <algorithm>

namespace redgem {
namespace {

// The nearest group with a reverb gives it (none was found yet).
bool takes_reverb(const GroupEffects& own, const VoiceEffects& found) {
  if (found.reverb > kNoReverb) return false;
  return own.reverb > kNoReverb;
}

bool lower_cutoff(std::optional<float> candidate, std::optional<float> current) {
  if (!candidate) return false;
  return !current || *candidate < *current;
}

}  // namespace

MixGroupTree::MixGroupTree() {
  for (const MixGroup group : kMixGroups) groups_.push_back(Group{mix_group_name(group)});
}

std::optional<MixGroup> MixGroupTree::define(const std::string& name, MixGroup parent, float volume) {
  if (!may_define(name, parent)) return std::nullopt;
  const auto existing = find(name);
  const MixGroup group = existing.value_or(static_cast<MixGroup>(groups_.size()));
  if (!existing) groups_.push_back(Group{name});
  at(group).parent = parent;
  at(group).volume = volume;
  return group;
}

// A name that is not the engine's, under a group there is: a group of the game's not under the
// one it moves to (that would make a loop), or a new one while there is room.
bool MixGroupTree::may_define(const std::string& name, MixGroup parent) const {
  if (!known(parent)) return false;
  if (mix_group_from_name(name)) return false;
  const auto existing = find(name);
  if (existing) return !within(parent, *existing);
  return groups_.size() < kMaxMixGroups;
}

// Whether a walk up the tree is still on a group.
bool MixGroupTree::on_group(std::optional<MixGroup> on) const { return on.has_value() && known(*on); }

std::optional<MixGroup> MixGroupTree::find(const std::string& name) const {
  const auto found = std::find_if(groups_.begin(), groups_.end(), [&](const Group& group) { return group.name == name; });
  if (found == groups_.end()) return std::nullopt;
  return static_cast<MixGroup>(found - groups_.begin());
}

void MixGroupTree::forget_game_groups() {
  groups_.resize(kMixGroupCount);
  for (Group& group : groups_) group.effects = GroupEffects{};
}

void MixGroupTree::set_player_volume(MixGroup group, float volume) {
  if (known(group)) at(group).volume = volume;
}

float MixGroupTree::player_volume(MixGroup group) const { return known(group) ? at(group).volume : kFullVolume; }

bool MixGroupTree::set_gain(MixGroup group, float gain) {
  if (!known(group)) return false;
  if (at(group).gain == gain) return false;
  at(group).gain = gain;
  return true;
}

bool MixGroupTree::set_effects(MixGroup group, const GroupEffects& effects) {
  if (!known(group)) return false;
  if (!valid_group_effects(effects)) return false;
  at(group).effects = effects;
  return true;
}

float MixGroupTree::heard_volume(MixGroup group) const {
  float heard = kFullVolume;
  for (std::optional<MixGroup> on = group; on_group(on); on = at(*on).parent) heard *= at(*on).volume * at(*on).gain;
  return heard;
}

VoiceEffects MixGroupTree::effects_of(MixGroup group) const {
  VoiceEffects effects;
  for (std::optional<MixGroup> on = group; on_group(on); on = at(*on).parent) {
    const GroupEffects& own = at(*on).effects;
    if (lower_cutoff(own.low_pass_hz, effects.low_pass_hz)) effects.low_pass_hz = own.low_pass_hz;
    if (takes_reverb(own, effects)) {
      effects.reverb = own.reverb;
      effects.reverb_bus = *on;
    }
  }
  return effects;
}

bool MixGroupTree::within(MixGroup group, MixGroup over) const {
  for (std::optional<MixGroup> on = group; on_group(on); on = at(*on).parent) {
    if (*on == over) return true;
  }
  return false;
}

}  // namespace redgem
