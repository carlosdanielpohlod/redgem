#include "app/scene_activation.hpp"

#include <utility>

#include "app/scene_bodies.hpp"

namespace redgem {

SceneActivation::SceneActivation(std::string name, SceneDesc desc, std::unique_ptr<PhysicsWorld> physics,
                                 int generation)
    : name_(std::move(name)),
      desc_(std::move(desc)),
      scene_(Scene::from_desc(desc_)),
      physics_(std::move(physics)),
      waiting_(scene_.alive_ids()),
      generation_(generation) {}

bool SceneActivation::advance(const FrameBudget& budget, Presenter& presenter, std::size_t upload_bytes) {
  if (!bodies_made()) {
    make_bodies(budget);
    return false;  // (the frames after the bodies upload)
  }
  return presenter.prepare_scene(desc_, generation_, upload_bytes);
}

void SceneActivation::make_all_bodies() {
  while (!bodies_made()) make_next_body();
}

void SceneActivation::make_bodies(const FrameBudget& budget) {
  // One body each frame at least, however little time is left, so that the scene does get there.
  while (!bodies_made()) {
    make_next_body();
    if (budget.spent()) return;
  }
}

void SceneActivation::make_next_body() { bodies::create({scene_, desc_}, *physics_, waiting_[next_body_++]); }

}  // namespace redgem
