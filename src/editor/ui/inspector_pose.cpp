// Inspector card that poses an object by one of the scene's clips, in the Scene view only, while
// the game is stopped (see editor/pose_preview.hpp).

#include <string>
#include <vector>

#include "core/translations.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::inspector {
namespace {

constexpr int kRestChoice = 0;  // the first choice of the list: no clip, the rest pose
constexpr const char* kClipId = "##pose_clip";
constexpr const char* kSecondsId = "##pose_seconds";
constexpr const char* kSecondsFormat = "%.2f s";

// The pose the preview shows on `index`, or null when it poses nothing there.
const pose_preview::Pose* pose_on(const EditorSession& session, int index) {
  const auto& pose = session.pose_preview();
  if (!pose.has_value()) return nullptr;
  return pose->object == index ? &*pose : nullptr;
}

// The clip the card shows for `index`: the one posing it now, or none.
int shown_clip(const EditorSession& session, int index) {
  const pose_preview::Pose* pose = pose_on(session, index);
  return pose != nullptr ? pose->clip : kNoIndex;
}

// The rest pose, then each clip by name; `chosen` is where the shown one is in that list.
std::vector<std::string> clip_choices(const EditorSession& session, const std::vector<int>& clips, int shown,
                                      int& chosen) {
  std::vector<std::string> names{tr("editor.inspector.pose.rest")};
  chosen = kRestChoice;
  for (std::size_t i = 0; i < clips.size(); ++i) {
    if (clips[i] == shown) chosen = static_cast<int>(i) + 1;
    names.push_back(pose_preview::clip_name(session.preview_desc(), clips[i]));
  }
  return names;
}

void choose_clip(EditorSession& session, int index, const std::vector<int>& clips, int shown) {
  int chosen = kRestChoice;
  const std::vector<std::string> names = clip_choices(session, clips, shown, chosen);
  std::vector<const char*> items;
  items.reserve(names.size());
  for (const std::string& name : names) items.push_back(name.c_str());
  property_label(tr("editor.inspector.pose.clip"));
  if (!ImGui::Combo(kClipId, &chosen, items.data(), static_cast<int>(items.size()))) return;
  if (chosen == kRestChoice) return session.end_pose_preview();
  session.preview_pose(index, clips[static_cast<std::size_t>(chosen) - 1], 0.0f);
}

void scrub(EditorSession& session, int index, const pose_preview::Pose& pose) {
  const AnimationDesc& clip = session.preview_desc().animations[static_cast<std::size_t>(pose.clip)];
  float seconds = pose.seconds;
  property_label(tr("editor.inspector.pose.time"));
  if (ImGui::SliderFloat(kSecondsId, &seconds, 0.0f, clip.duration, kSecondsFormat)) {
    session.preview_pose(index, pose.clip, seconds);
  }
}

}  // namespace

void draw_pose(PanelContext& context, int index) {
  EditorSession& session = context.session;
  const std::vector<int> clips = session.clips_moving(index);
  if (clips.empty()) return;
  if (!component_header(ICON_FA_PERSON_WALKING, tr("editor.inspector.pose.title"))) return;
  choose_clip(session, index, clips, shown_clip(session, index));
  if (const pose_preview::Pose* pose = pose_on(session, index)) scrub(session, index, *pose);
  ImGui::PushStyleColor(ImGuiCol_Text, muted_color());
  ImGui::TextWrapped("%s", tr("editor.inspector.pose.note"));
  ImGui::PopStyleColor();
}

}  // namespace redgem::ui::inspector
