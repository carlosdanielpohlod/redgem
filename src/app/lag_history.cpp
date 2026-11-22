#include "app/lag_history.hpp"

#include <algorithm>

#include "core/math.hpp"

namespace redgem {
namespace {

// Seconds of history kept past the longest rewind: the others are shown a little behind the host
// (NetworkSync's delay), so what a shooter saw is that much older than when it shot.
constexpr double kShownBehindMargin = 0.5;

// How many objects up the object hangs from.
int depth_of(const Scene& scene, ObjectId id) {
  int depth = 0;
  for (ObjectId at = scene.at(id).parent; at != kNoObject; at = scene.at(at).parent) ++depth;
  return depth;
}

// Every object under `root` that has a body, at any depth.
void bodies_under(const Scene& scene, const PhysicsWorld& physics, ObjectId root, std::vector<ObjectId>& found) {
  for (const ObjectId child : scene.at(root).children) {
    if (physics.has_object(child)) found.push_back(child);
    bodies_under(scene, physics, child, found);
  }
}

Transform between(const Transform& from, const Transform& to, float share) {
  Transform drawn = to;
  drawn.position = mix(from.position, to.position, share);
  drawn.rotation = mix(from.rotation, to.rotation, share);
  return drawn;
}

// `later`'s poses, each `share` of the way from `earlier`'s of the same object (when it had one).
std::vector<std::pair<ObjectId, Transform>> blended(const std::vector<std::pair<ObjectId, Transform>>& earlier,
                                                    const std::vector<std::pair<ObjectId, Transform>>& later,
                                                    float share) {
  std::vector<std::pair<ObjectId, Transform>> poses = later;
  for (auto& [id, pose] : poses) {
    const auto same = [id = id](const std::pair<ObjectId, Transform>& other) { return other.first == id; };
    const auto before = std::find_if(earlier.begin(), earlier.end(), same);
    if (before != earlier.end()) pose = between(before->second, pose, share);
  }
  return poses;
}

}  // namespace

void LagHistory::track(const Scene& scene, ObjectId id) {
  const ObjectHandle handle = scene.handle(id);
  if (std::find(tracked_.begin(), tracked_.end(), handle) == tracked_.end()) tracked_.push_back(handle);
}

LagHistory::Sample LagHistory::sample_of(const Scene& scene, const PhysicsWorld& physics, ObjectId root,
                                         double now) const {
  const auto shown = shown_.find(root);
  Sample sample{root, shown != shown_.end() ? shown->second : now, {{root, scene.world_transform(root)}}};
  std::vector<ObjectId> bodies;
  bodies_under(scene, physics, root, bodies);
  for (const ObjectId body : bodies) sample.poses.emplace_back(body, scene.world_transform(body));
  return sample;
}

void LagHistory::record(const Scene& scene, const PhysicsWorld& physics, double now) {
  const auto gone = [&scene](ObjectHandle handle) { return scene.resolve(handle) == kNoObject; };
  tracked_.erase(std::remove_if(tracked_.begin(), tracked_.end(), gone), tracked_.end());
  Moment moment{now, {}};
  for (const ObjectHandle handle : tracked_) {
    moment.samples.push_back(sample_of(scene, physics, scene.resolve(handle), now));
  }
  moments_.push_back(std::move(moment));
  shown_.clear();
  forget_older_than(now);
}

void LagHistory::forget_older_than(double now) {
  const double oldest = now - longest_ - kShownBehindMargin;
  while (moments_.size() > 1 && moments_.front().now < oldest) moments_.pop_front();
}

void LagHistory::clear() {
  tracked_.clear();
  shown_.clear();
  moments_.clear();
  kept_.clear();
  moved_bodies_.clear();
  rewound_ = false;
}

double LagHistory::clamped(double asked, double now) const { return std::clamp(asked, now - longest_, now); }

std::vector<std::pair<ObjectId, Transform>> LagHistory::poses_at(ObjectId root, double time) const {
  const Sample* earlier = nullptr;
  for (const Moment& moment : moments_) {
    const auto it = std::find_if(moment.samples.begin(), moment.samples.end(),
                                 [root](const Sample& sample) { return sample.root == root; });
    if (it == moment.samples.end()) continue;
    if (it->shown_at < time) {
      earlier = &*it;
      continue;
    }
    if (earlier == nullptr || it->shown_at <= earlier->shown_at) return it->poses;
    const double share = (time - earlier->shown_at) / (it->shown_at - earlier->shown_at);
    return blended(earlier->poses, it->poses, static_cast<float>(share));
  }
  return earlier != nullptr ? earlier->poses : std::vector<std::pair<ObjectId, Transform>>{};
}

std::vector<LagHistory::Placement> LagHistory::planned(const Scene& scene, double time) const {
  std::vector<Placement> placements;
  for (const ObjectHandle handle : tracked_) {
    const ObjectId root = scene.resolve(handle);
    if (root == kNoObject) continue;
    for (const auto& [id, world] : poses_at(root, time)) {
      if (scene.is_alive(id)) placements.push_back({depth_of(scene, id), id, world});
    }
  }
  // (A parent first: placing it moves what hangs from it, which is placed after.)
  std::stable_sort(placements.begin(), placements.end(),
                   [](const Placement& a, const Placement& b) { return a.depth < b.depth; });
  return placements;
}

bool LagHistory::rewind(Scene& scene, PhysicsWorld& physics, double time) {
  if (rewound_) return false;
  rewound_ = true;
  for (const Placement& placement : planned(scene, time)) {
    kept_.emplace_back(placement.id, scene.at(placement.id).local);
    scene.set_world_transform(placement.id, placement.world);
    if (physics.has_object(placement.id)) moved_bodies_.push_back(placement.id);
  }
  for (const ObjectId body : moved_bodies_) physics.teleport(body, scene.world_transform(body));
  return true;
}

void LagHistory::restore(Scene& scene, PhysicsWorld& physics) {
  // (Last placed first: each gets back the local it had.)
  for (auto kept = kept_.rbegin(); kept != kept_.rend(); ++kept) {
    if (scene.is_alive(kept->first)) scene.at(kept->first).local = kept->second;
  }
  for (const ObjectId body : moved_bodies_) {
    if (scene.is_alive(body)) physics.teleport(body, scene.world_transform(body));
  }
  kept_.clear();
  moved_bodies_.clear();
  rewound_ = false;
}

}  // namespace redgem
