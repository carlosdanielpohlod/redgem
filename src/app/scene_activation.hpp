#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "app/presenter.hpp"
#include "core/frame_budget.hpp"
#include "physics/physics_world.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// What making a scene ready may take of each loading frame: about this much of its time (a
// frame of a 60 Hz game is 16.7 ms), and this many bytes sent to the graphics card.
inline constexpr double kActivationSecondsPerFrame = 0.004;
inline constexpr std::size_t kActivationUploadBytesPerFrame = std::size_t{8} * 1024 * 1024;

// A scene that was read, made ready to run a share at a time while the scene before it still runs
// and the loading screen is drawn: its objects when it is made, then its bodies, a batch a frame
// within the frame's time, then what it is drawn with, a bounded number of bytes a frame. Its
// scripts start once it is all ready, when it takes the running scene's place (Engine::switch_to).
class SceneActivation {
 public:
  // `physics` is the empty world its bodies go into; `generation` the one it will be shown under.
  SceneActivation(std::string name, SceneDesc desc, std::unique_ptr<PhysicsWorld> physics, int generation);

  // Does this frame's share: at least one piece of work, then more while `budget` lasts, and at
  // most `upload_bytes` (at least one asset) sent to `presenter`. True once all of it is done.
  bool advance(const FrameBudget& budget, Presenter& presenter, std::size_t upload_bytes);
  // Gives every object its body now, however long it takes.
  void make_all_bodies();

  const std::string& name() const { return name_; }
  int generation() const { return generation_; }
  SceneDesc& desc() { return desc_; }
  Scene& scene() { return scene_; }
  std::unique_ptr<PhysicsWorld> take_physics() { return std::move(physics_); }

 private:
  // Makes bodies while the frame's time lasts, one at least.
  void make_bodies(const FrameBudget& budget);
  void make_next_body();
  bool bodies_made() const { return next_body_ >= waiting_.size(); }
  bool time_left_to_upload(const FrameBudget& budget) const;

  std::string name_;
  SceneDesc desc_;
  Scene scene_;  // (made of desc_, so after it)
  std::unique_ptr<PhysicsWorld> physics_;
  std::vector<ObjectId> waiting_;  // the objects whose bodies are made, in this order
  std::size_t next_body_ = 0;
  int generation_ = 0;
};

}  // namespace redgem
