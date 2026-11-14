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
    // (The frame the last bodies took all of uploads nothing: the next one does.)
    if (!time_left_to_upload(budget)) return false;
  }
  if (!presenter.prepare_scene(desc_, generation_, upload_bytes, budget)) return false;
  // A presenter that draws the pictures has them on the graphics card by now.
  if (presenter.draws_pictures()) desc_.decoded_images = {};
  return true;
}

bool SceneActivation::time_left_to_upload(const FrameBudget& budget) const {
  return bodies_made() && !budget.spent();
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
