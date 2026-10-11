#pragma once

#include <cstdint>
#include <map>
#include <unordered_set>
#include <optional>
#include <string>
#include <vector>

#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// Plays a scene's animation clips (Animation in Ruby). Every clip starts out playing in a loop, as
// a scene with moving parts should look alive without a line of code; scripts pause, stop, restart
// and change the speed of each one by its name.
// A character can also be given a clip of its own (game_object.animation in Ruby): it then stops
// following the scene's clips and plays that one, fading over from what it was doing. A character
// is an object and what is under it, or a spawned copy as a whole.
struct PoseOptions;

// How a character is to start a clip (see AnimationPlayer::play_on).
struct ClipStart {
  bool loop = true;
  float speed = 1.0f;    // a negative speed plays backwards
  float fade = 0.0f;     // seconds taken to reach the clip from what the character was doing
  bool restart = false;  // from the beginning, even when already on this clip
};

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

 private:
  struct State {
    float time = 0.0f;
    float speed = 1.0f;
    bool playing = true;
    bool loop = true;
    bool posing = true;  // false once stopped: the clip no longer touches its objects
  };

  // A clip being played by one character.
  struct Track {
    std::size_t clip = 0;
    State state;
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
  };

  std::optional<std::size_t> find(const std::string& name) const;
  void advance_state(State& state, float duration, float dt) const;
  // Sets a clip playing: from where it is, or from its beginning (its end, played backwards).
  static void start(State& state, float duration, bool loop, float speed, bool restart);
  // The object characters are told apart by: the root of a copy, or the object itself.
  static ObjectId character_of(const Scene& scene, ObjectId object);
  // The character with a clip of its own that `object` is part of, or kNoObject.
  ObjectId owner_of(const Scene& scene, ObjectId object) const;
  const Track* track_on(const Scene& scene, ObjectId object) const;
  // Lays what a character is coming from or going to over its objects.
  void pose_character(ObjectId character, const std::optional<Track>& track, float weight, Scene& scene);
  // Poses everything that follows the scene's clips.
  void pose_followers(Scene& scene);
  // Lays every scene clip that is posing over the objects `options` wants.
  void apply_scene_clips(Scene& scene, PoseOptions options) const;
  void advance_track(std::optional<Track>& track, float dt) const;
  // Moves one character's own clips on and poses it. True once nothing of its own is left.
  bool advance_own(ObjectId character, Own& own, float dt, Scene& scene);
  // Sets a character on its way to `next` (the scene's clips when nullopt) over `fade` seconds.
  static void fade_to(Own& own, const std::optional<Track>& next, float fade);
  // What it takes to lay a clip over one character only, by `weight`.
  PoseOptions character_options(ObjectId character, float weight, const Scene& scene);
  // Called before a part of an object is posed by `weight`: a blend gets its steady base, and
  // either way the part counts as posed this frame.
  void before_posing(const Scene& scene, ObjectId object, AnimationPath path, Transform& local, float weight);

  const std::vector<AnimationDesc>* clips_ = nullptr;
  // Makes sure a blend starts from something steady: what was laid on that part of the object
  // earlier this frame, or its pose at rest; never what the blend itself left there last frame.
  void steady_base(const Scene& scene, ObjectId object, AnimationPath path, Transform& local);

  std::vector<State> states_;
  std::map<ObjectId, Own> owned_;
  std::vector<Transform> rest_;              // by object id, as the scene was loaded
  std::unordered_set<std::uint64_t> posed_;  // (object, path) pairs posed so far this frame
};

}  // namespace redgem
