#pragma once

// The groups a game's sounds are mixed in, each with a volume of its own: a settings screen turns
// the music down without the footsteps. The master volume is over all of them. Three groups are
// the engine's (music, effects, voice: the player's settings turn them); a game adds its own under
// them (Audio.group(:cave, parent: :effects)), each with its volume and its effects.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace redgem {

// What the audio is at before a game changes it: every sound and music at its full volume, the
// music at the pitch it was recorded at.
inline constexpr float kFullVolume = 1.0f;
inline constexpr float kAsRecorded = 1.0f;

// The engine's groups by name; a game's groups follow them, numbered from kMixGroupCount.
enum class MixGroup : std::uint8_t { Music, Effects, Voice };
inline constexpr std::size_t kMixGroupCount = 3;  // the engine's
inline constexpr std::array<MixGroup, kMixGroupCount> kMixGroups = {MixGroup::Music, MixGroup::Effects,
                                                                    MixGroup::Voice};
// Every group at once, the engine's and a game's: a game has 13 of its own at most.
inline constexpr std::size_t kMaxMixGroups = 16;
// A volume for each of the engine's groups, in kMixGroups' order.
using GroupVolumes = std::array<float, kMixGroupCount>;
inline constexpr GroupVolumes kFullGroupVolumes = {kFullVolume, kFullVolume, kFullVolume};

inline std::size_t mix_group_index(MixGroup group) { return static_cast<std::size_t>(group); }

// "music", "effects", "voice", as scripts name them.
const char* mix_group_name(MixGroup group);
std::optional<MixGroup> mix_group_from_name(const std::string& name);

}  // namespace redgem
