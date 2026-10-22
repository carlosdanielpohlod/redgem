#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "scene/animation/character_animation.hpp"
#include "scene/animation/animation_graph_player.hpp"
#include "scene/animation/animation_blend.hpp"
#include "scene/animation/clip_clock.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

struct PoseOptions;

// How a character is to start a clip (see AnimationPlayer::play_on).
struct ClipStart {
  bool loop = true;
  float speed = 1.0f;    // a negative speed plays backwards
  float fade = 0.0f;     // seconds taken to reach the clip from what the character was doing
  bool restart = false;  // from the beginning, even when already on this clip
};

// Plays a scene's animation clips (Animation in Ruby). Every clip starts out playing in a loop, as
// a scene with moving parts should look alive without a line of code; scripts pause, stop, restart
// and change the speed of each one by its name.
// A character can also be given a clip of its own (game_object.animation in Ruby): it then stops
// following the scene's clips and plays that one, fading over from what it was doing. A character
// is an object and what is under it, or a spawned copy as a whole.
// Implemented in parts: animation_player.cpp (the scene's clips, by name),
// animation_player_characters.cpp (a character's own clip), animation_player_posing.cpp (what
// every frame does: moving the clips on and posing the objects) and animation_player_sync.cpp
// (a character's clips as other machines see them).
class AnimationPlayer {
 public:
  // Starts over with a scene's clips, which must outlive the player (or the next reset). The
  // poses the scene's objects are in right now are remembered as their poses at rest.
  void reset(const std::vector<AnimationDesc>& clips, const Scene& scene);
  // Moves the playing clips on by dt seconds and poses the scene's objects.
  void advance(float dt, Scene& scene);

  std::vector<std::string> names() const;
  bool has(const std::string& name) const { return find(name).has_value(); }
  // False for an unknown clip, like the calls below. `restart` plays from the beginning (a clip
  // that ended starts over anyway); a negative speed plays backwards.
  bool play(const std::string& name, bool loop, float speed, bool restart);
  // Freezes the clip where it is.
  bool pause(const std::string& name);
  // Lets go of the objects: they stay as they are and scripts may move them.
  bool stop(const std::string& name);
  bool seek(const std::string& name, float seconds);
  std::optional<bool> playing(const std::string& name) const;
  std::optional<float> time(const std::string& name) const;
  std::optional<float> duration(const std::string& name) const;

  // --- One character -----------------------------------------------------------------------
  // Plays a clip on the character `object` belongs to, reaching it over `how.fade` seconds from
  // what the character was doing. False for an unknown clip.
  bool play_on(const Scene& scene, ObjectId object, const std::string& name, const ClipStart& how);
  // Freezes the character's own clip where it is.
  void pause_on(const Scene& scene, ObjectId object);
  // Lays a second clip over what the character is doing, by `weight` (0 to 1): half a limp over a
  // walk, or a wave that only moves the arm its channels are about. False for an unknown clip.
  bool mix_on(const Scene& scene, ObjectId object, const std::string& name, float weight, bool loop, float speed);
  void unmix_on(const Scene& scene, ObjectId object);
  // Hands the character back to the scene's clips, over `fade` seconds.
  void follow_scene(const Scene& scene, ObjectId object, float fade);
  // The character's own clip (the one it is fading to, while fading); nullopt while it follows the scene.
  std::optional<std::string> clip_on(const Scene& scene, ObjectId object) const;
  bool playing_on(const Scene& scene, ObjectId object) const;
  float time_on(const Scene& scene, ObjectId object) const;

  bool use_graph(const Scene& scene, ObjectId object, const AnimationGraphDefinition& graph, std::string& error);
  bool set_parameter(const Scene& scene, ObjectId object, const std::string& name, const AnimationParameter& value);
  std::optional<std::string> graph_state(const Scene& scene, ObjectId object) const;

  // --- What other machines see (NetworkSync) ------------------------------------------------
  // What the character `object` belongs to does of its own, for the others.
  CharacterAnimation animation_of(const Scene& scene, ObjectId object) const;
  // Its revision alone (0 while no script ever changed what it does).
  std::uint32_t revision_of(const Scene& scene, ObjectId object) const;
  // Makes the character do what `state` says another machine's does, `late_by` seconds on from
  // when it said so: the same clips, from the same moment in them. A clip this scene does not
  // have is left out.
  void take_animation(const Scene& scene, ObjectId object, const CharacterAnimation& state, float late_by);

 private:
  // A clip being played by one character.
  struct Track {
    std::size_t clip = 0;
    ClipClock clock;
  };
  // What one character does instead of following the scene: `from` on its way to `to` (either
  // one may be "the scene's clips": nullopt), `faded` of `fade` seconds into the change.
  struct Own {
    std::optional<Track> from;
    std::optional<Track> to;
    float fade = 0.0f;
    float faded = 0.0f;
    // A clip laid over all that (see mix_on).
    std::optional<Track> layer;
    float layer_weight = 0.0f;
    std::optional<AnimationGraphPlayer> graph;
    std::optional<GraphAnimation> received_graph;
  };

  static constexpr float kWhole = 1.0f;  // a clip laid on with all its weight

  std::optional<std::size_t> find(const std::string& name) const;
  float duration_of(std::size_t clip) const { return (*clips_)[clip].duration; }

  // The object characters are told apart by: the root of a copy, or the object itself.
  static ObjectId character_of(const Scene& scene, ObjectId object);
  // The character with a clip of its own that `object` is part of, or kNoObject.
  ObjectId owner_of(const Scene& scene, ObjectId object) const;
  // What the character `object` belongs to does on its own; null while it only follows the scene.
  Own* own_of(const Scene& scene, ObjectId object);
  const Own* own_of(const Scene& scene, ObjectId object) const;
  const Track* track_on(const Scene& scene, ObjectId object) const;
  // The character's own clip, and the clip mixed over it; null when it has none.
  Track* own_track(const Scene& scene, ObjectId object);
  Track* mixed_track(const Scene& scene, ObjectId object);
  // The clock of `track` when it plays clip `clip`; null otherwise.
  static ClipClock* clock_on(std::optional<Track>& track, std::size_t clip);
  // Sets a character on its way to `next` (the scene's clips when nullopt) over `fade` seconds.
  static void fade_to(Own& own, const std::optional<Track>& next, float fade);
  // A script changed what the character does: its revision moves on (see CharacterAnimation).
  void changed(const Scene& scene, ObjectId object);
  ClipState state_of(const Track& track) const;
  // Sets a track's clock to what another machine said of it, `late_by` seconds on.
  void catch_up(Track& track, const ClipState& state, float late_by) const;
  void take_own(const Scene& scene, ObjectId object, const CharacterAnimation& state, float late_by);
  // Puts the character on `state`'s own clip; false for a clip this scene does not have.
  bool onto(const Scene& scene, ObjectId object, const ClipState& own, float fade);
  void take_layer(const Scene& scene, ObjectId object, const CharacterAnimation& state, float late_by);
  void advance_graph(ObjectId character, Own& own, float dt, Scene& scene);
  GraphAnimation graph_animation(const Own& own) const;
  bool take_graph(const Scene& scene, ObjectId object, const GraphAnimation& graph, float late_by);
  void pose_graph(ObjectId character, const GraphAnimation& graph, Scene& scene);
  void tick_graph(GraphAnimation& graph, float dt) const;
  void tick_graph_side(std::vector<WeightedClipState>& clips, float dt) const;
  static bool has_graph(const Own& own) { return own.graph.has_value() || own.received_graph.has_value(); }
  static bool graph_paused(const Own& own);
  static void detach_graph(Own& own);
  bool leave_graph(const Scene& scene, ObjectId object);
  bool pause_graph_on(const Scene& scene, ObjectId object);
  std::optional<ClipState> dominant_graph_clip(const Scene& scene, ObjectId object) const;
  bool known_graph_clip(const WeightedClipState& entry) const;
  bool known_graph_side(const std::vector<WeightedClipState>& side) const;

  // Poses everything that follows the scene's clips.
  void pose_followers(Scene& scene);
  // Lays every scene clip that is posing over the objects `options` wants.
  void apply_scene_clips(Scene& scene, PoseOptions options) const;
  void advance_track(std::optional<Track>& track, float dt) const;
  // Moves one character's own clips on and poses it. True once nothing of its own is left.
  bool advance_manual_own(ObjectId character, Own& own, float dt, Scene& scene);
  bool advance_own(ObjectId character, Own& own, float dt, Scene& scene);
  // Lays what a character is coming from or going to over its objects.
  void pose_character(ObjectId character, const std::optional<Track>& track, float weight, Scene& scene);
  // What it takes to lay a clip over one character only, by `weight`.
  PoseOptions character_options(ObjectId character, float weight, const Scene& scene);
  // Called before a part of an object is posed by `weight`: a blend gets its steady base, and
  // either way the part counts as posed this frame.
  void before_posing(const Scene& scene, ObjectId object, AnimationPath path, Transform& local, float weight);
  // Makes sure a blend starts from something steady: what was laid on that part of the object
  // earlier this frame, or its pose at rest; never what the blend itself left there last frame.
  void steady_base(const Scene& scene, ObjectId object, AnimationPath path, Transform& local);

  const std::vector<AnimationDesc>* clips_ = nullptr;
  std::vector<ClipClock> clocks_;                    // one for each of the scene's clips
  std::map<ObjectHandle, Own> owned_;                // by character: a new object in its slot is someone else
  std::map<ObjectHandle, std::uint32_t> revisions_;  // by character, from its first change on
  std::vector<Transform> rest_;                      // by object id, as the scene was loaded
  AnimationBlendLayout blend_layout_;
  std::unordered_set<std::uint64_t> posed_;  // (object, path) pairs posed so far this frame
};

}  // namespace redgem
