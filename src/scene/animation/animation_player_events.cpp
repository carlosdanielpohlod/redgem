// The moments of a character's own clips that scripts listen for (animation.on): which ones each
// advance went past, queued for the scripts' update.

#include <algorithm>
#include <iterator>
#include <utility>

#include "scene/animation/animation_markers.hpp"
#include "scene/animation/animation_player.hpp"

namespace redgem {
namespace {

// Drops what was kept for characters that are gone.
template <typename ByCharacter>
void erase_departed(const Scene& scene, ByCharacter& kept) {
  for (auto entry = kept.begin(); entry != kept.end();) {
    entry = scene.resolve(entry->first) == kNoObject ? kept.erase(entry) : std::next(entry);
  }
}

}  // namespace

std::uint32_t AnimationPlayer::listen_at(const Scene& scene, ObjectId object, const std::string& name, float at) {
  const auto index = find(name);
  if (!index) return kNoAnimationListener;
  return listen(scene, object, Listener{kNoAnimationListener, scene.handle(object), index, at, {}});
}

std::uint32_t AnimationPlayer::listen_marker(const Scene& scene, ObjectId object, const std::string& marker) {
  return listen(scene, object, Listener{kNoAnimationListener, scene.handle(object), std::nullopt, 0.0f, marker});
}

std::uint32_t AnimationPlayer::listen(const Scene& scene, ObjectId object, Listener listener) {
  listener.id = next_listener_++;
  listeners_[scene.handle(character_of(scene, object))].push_back(listener);
  return listener.id;
}

void AnimationPlayer::unlisten(std::uint32_t listener) {
  for (auto& [character, listeners] : listeners_) {
    const auto gone = [listener](const Listener& each) { return each.id == listener; };
    listeners.erase(std::remove_if(listeners.begin(), listeners.end(), gone), listeners.end());
  }
}

std::vector<AnimationEvent> AnimationPlayer::take_events() { return std::exchange(events_, {}); }

void AnimationPlayer::forget_scripts() {
  listeners_.clear();
  root_motion_.clear();
  events_.clear();
  motions_.clear();
}

void AnimationPlayer::forget_departed(const Scene& scene) {
  erase_departed(scene, listeners_);
  erase_departed(scene, root_motion_);
  for (auto& [character, listeners] : listeners_) {
    const auto gone = [&scene](const Listener& each) { return scene.resolve(each.object) == kNoObject; };
    listeners.erase(std::remove_if(listeners.begin(), listeners.end(), gone), listeners.end());
  }
}

// --- What a character showed in the last advance ----------------------------------------------

bool AnimationPlayer::fade_arrived(const Own& own) { return own.fade <= 0.0f || own.faded >= own.fade; }

AnimationPlayer::ClipStep AnimationPlayer::step_of(const Track& track, float weight) {
  return {track.clip, track.span, track.clock.loop, weight};
}

AnimationPlayer::CharacterSteps AnimationPlayer::steps_of(const Own& own) const {
  CharacterSteps steps;
  if (own.layer) steps.layer = step_of(*own.layer, own.layer_weight);
  if (own.graph) {
    graph_steps(*own.graph, steps);
  } else if (!own.received_graph) {
    manual_steps(own, steps);
  }
  return steps;
}

void AnimationPlayer::manual_steps(const Own& own, CharacterSteps& steps) {
  if (own.to) steps.to.push_back(step_of(*own.to, kWhole));
  if (fade_arrived(own)) return;
  if (own.from) steps.from.push_back(step_of(*own.from, kWhole));
  steps.blend = own.faded / own.fade;
}

void AnimationPlayer::graph_steps(const AnimationGraphPlayer& graph, CharacterSteps& steps) const {
  const AnimationGraphFrame frame = graph.frame();
  steps.to = sample_steps(frame.to, graph.current_span());
  if (const auto source = graph.source_span()) steps.from = sample_steps(frame.from, *source);
  steps.blend = frame.blend;
}

std::vector<AnimationPlayer::ClipStep> AnimationPlayer::sample_steps(const std::vector<AnimationGraphSample>& samples,
                                                                     ClockSpan phases) const {
  std::vector<ClipStep> steps;
  for (const AnimationGraphSample& sample : samples) {
    const double seconds = duration_of(sample.clip);
    const ClockSpan span{phases.from * seconds, phases.to * seconds, phases.laps};
    steps.push_back({sample.clip, span, sample.loop, sample.weight});
  }
  return steps;
}

// Only characters scripts listen to, or that root motion moves, are looked at after advancing.
bool AnimationPlayer::followed_up(ObjectHandle character) const {
  return listeners_.count(character) != 0 || root_motion_.count(character) != 0;
}

void AnimationPlayer::follow_up(ObjectId character, const Own& own, Scene& scene) {
  if (!followed_up(scene.handle(character))) return;
  const CharacterSteps steps = steps_of(own);
  queue_events(scene, character, steps);
  move_by_root(character, steps, scene);
}

// --- Events -----------------------------------------------------------------------------------

namespace {

template <typename Step>
bool lighter(const Step& a, const Step& b) {
  return a.weight < b.weight;
}

// A clip mixed over a character fires its events while it shows enough of it, unless it is the
// clip that leads already (whose events fire once).
template <typename Step>
bool layer_fires(const std::optional<Step>& layer, const Step* main, float least) {
  if (!layer.has_value() || layer->weight < least) return false;
  return main == nullptr || main->clip != layer->clip;
}

}  // namespace

void AnimationPlayer::queue_events(const Scene& scene, ObjectId character, const CharacterSteps& steps) {
  const auto found = listeners_.find(scene.handle(character));
  if (found == listeners_.end()) return;
  const auto lead = std::max_element(steps.to.begin(), steps.to.end(), lighter<ClipStep>);  // the first of equals
  const ClipStep* main = lead != steps.to.end() ? &*lead : nullptr;
  if (main != nullptr) queue_step_events(found->second, *main);
  if (layer_fires(steps.layer, main, kLayerEventWeight)) queue_step_events(found->second, *steps.layer);
}

void AnimationPlayer::queue_step_events(const std::vector<Listener>& listeners, const ClipStep& step) {
  const std::vector<ListenedMark> marks = marks_of(listeners, step.clip);
  if (marks.empty()) return;
  const double duration = duration_of(step.clip);
  std::vector<double> seconds;
  for (const ListenedMark& mark : marks) seconds.push_back(static_cast<double>(mark.at) * duration);
  const ClipSpan span{step.span.from, unwrapped_end(step.span, duration), duration, step.loop};
  for (const std::size_t index : marks_passed(span, seconds)) {
    const ListenedMark& mark = marks[index];
    events_.push_back({mark.listener->object, mark.listener->id, (*clips_)[step.clip].name, mark.name, mark.at});
  }
}

std::vector<AnimationPlayer::ListenedMark> AnimationPlayer::marks_of(const std::vector<Listener>& listeners,
                                                                     std::size_t clip) const {
  std::vector<ListenedMark> marks;
  for (const Listener& listener : listeners) {
    if (listener.clip == clip) marks.push_back({&listener, listener.at, {}});
    add_authored(listener, (*clips_)[clip], marks);
  }
  return marks;
}

void AnimationPlayer::add_authored(const Listener& listener, const AnimationDesc& clip,
                                   std::vector<ListenedMark>& marks) {
  if (listener.marker.empty()) return;
  for (const AnimationMarker& marker : clip.markers) {
    if (marker.name == listener.marker) marks.push_back({&listener, marker.at, marker.name});
  }
}

}  // namespace redgem
