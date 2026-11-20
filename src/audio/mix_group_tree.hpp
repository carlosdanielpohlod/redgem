#pragma once

// The mix groups of a game as a tree: the engine's three at the roots, a game's own under them.
// A sound is heard at the volumes of its group and of every group over it, and played with their
// effects.

#include <optional>
#include <string>
#include <vector>

#include "audio/audio_effects.hpp"
#include "audio/mix_groups.hpp"

namespace redgem {

class MixGroupTree {
 public:
  MixGroupTree();

  // A game's group under `parent`, at `volume` (full when none); the same name again moves it,
  // and changes its volume when one is given. nullopt for an engine group's name, a parent that
  // is under it, or no room left.
  std::optional<MixGroup> define(const std::string& name, MixGroup parent, std::optional<float> volume);
  // A group by the name scripts give it, the engine's or the game's.
  std::optional<MixGroup> find(const std::string& name) const;
  // Every group the game made goes, and every effect; the player's volumes and the gains stay.
  void forget_game_groups();

  // The player's volume of one of the engine's groups (Settings.music_volume...).
  void set_player_volume(MixGroup group, float volume);
  float player_volume(MixGroup group) const;
  // A gain the engine puts over a group (the music ducked under a voice); false when it did not
  // change.
  bool set_gain(MixGroup group, float gain);
  // False for effects that are not valid, which change nothing.
  bool set_effects(MixGroup group, const GroupEffects& effects);

  // What a sound of the group is heard at: the volumes and gains of it and every group over it.
  float heard_volume(MixGroup group) const;
  VoiceEffects effects_of(MixGroup group) const;
  // Whether `group` is `over` or under it.
  bool within(MixGroup group, MixGroup over) const;
  bool known(MixGroup group) const { return mix_group_index(group) < groups_.size(); }

 public:
  // One group as the tree keeps it.
  struct Group {
    std::string name;
    std::optional<MixGroup> parent;  // none for the engine's
    float volume = kFullVolume;      // the player's for the engine's, the game's for its own
    float gain = kFullVolume;
    GroupEffects effects;
  };

 private:
  Group& at(MixGroup group) { return groups_[mix_group_index(group)]; }
  const Group& at(MixGroup group) const { return groups_[mix_group_index(group)]; }
  bool may_define(const std::string& name, MixGroup parent) const;
  bool on_group(std::optional<MixGroup> on) const;

  std::vector<Group> groups_;
};

}  // namespace redgem
