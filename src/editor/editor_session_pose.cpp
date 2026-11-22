// Part of EditorSession: pose preview, an object shown posed by a clip in the Scene view while
// the game is stopped (see editor/pose_preview.hpp).

#include "editor/editor_session.hpp"

namespace redgem {
namespace {

// Whether showing `next` first needs the rest pose back: the pose shown moves another object or
// plays another clip, whose nodes `next` may not move.
bool replaces_pose(const std::optional<pose_preview::Pose>& shown, const pose_preview::Pose& next) {
  return shown && (shown->object != next.object || shown->clip != next.clip);
}

}  // namespace

std::vector<int> EditorSession::clips_moving(ObjectId id) const {
  return pose_preview::clips_moving(preview_desc(), id);
}

bool EditorSession::preview_pose(ObjectId id, int clip, float seconds) {
  const pose_preview::Pose pose{id, clip, seconds};
  if (!can_pose(pose)) return false;
  const bool from_rest = replaces_pose(pose_, pose);
  pose_ = pose;
  if (from_rest) {
    refresh_preview();  // (which shows the pose)
  } else {
    show_pose();
  }
  return true;
}

bool EditorSession::can_pose(const pose_preview::Pose& pose) const {
  return editing() && pose_preview::fits(preview_desc(), pose);
}

void EditorSession::end_pose_preview() {
  if (!pose_) return;
  pose_.reset();
  refresh_preview();
}

void EditorSession::show_pose() {
  if (pose_ && editing()) pose_preview::show(preview_desc(), *pose_, preview_);
}

void EditorSession::keep_pose_with_selection() {
  if (pose_ && selection() != pose_->object) end_pose_preview();
}

}  // namespace redgem
