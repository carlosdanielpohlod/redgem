#pragma once

#include <optional>
#include <string>
#include <vector>

#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// Plays a scene's animation clips (Animation in Ruby). Every clip starts out playing in a loop, as
// a scene with moving parts should look alive without a line of code; scripts pause, stop, restart
// and change the speed of each one by its name.
class AnimationPlayer {
 public:
  // Starts over with a scene's clips, which must outlive the player (or the next reset).
  void reset(const std::vector<AnimationDesc>& clips);
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

 private:
  struct State {
    float time = 0.0f;
    float speed = 1.0f;
    bool playing = true;
    bool loop = true;
    bool posing = true;  // false once stopped: the clip no longer touches its objects
  };

  std::optional<std::size_t> find(const std::string& name) const;

  const std::vector<AnimationDesc>* clips_ = nullptr;
  std::vector<State> states_;
};

}  // namespace redgem
