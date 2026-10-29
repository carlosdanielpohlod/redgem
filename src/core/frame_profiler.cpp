#include "core/frame_profiler.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace redgem {
namespace {
constexpr double kMillisecondsPerSecond = 1000.0;
constexpr std::array<std::string_view, kProfilePhaseCount> kPhaseNames{"input",   "network",      "http", "scripts",
                                                                       "physics", "presentation", "scene"};

double milliseconds_between(double start, double end) {
  const double elapsed = (end - start) * kMillisecondsPerSecond;
  const bool usable = std::isfinite(elapsed) && elapsed >= 0.0;
  return usable ? elapsed : 0.0;
}
void add(ProfileFrame& sum, const ProfileFrame& frame) {
  sum.frame_ms += frame.frame_ms;
  sum.interval_ms += frame.interval_ms;
  for (std::size_t i = 0; i < kProfilePhaseCount; ++i) sum.phase_ms[i] += frame.phase_ms[i];
}
void keep_peak(ProfileFrame& peak, const ProfileFrame& frame) {
  peak.frame_ms = std::max(peak.frame_ms, frame.frame_ms);
  peak.interval_ms = std::max(peak.interval_ms, frame.interval_ms);
  for (std::size_t i = 0; i < kProfilePhaseCount; ++i) peak.phase_ms[i] = std::max(peak.phase_ms[i], frame.phase_ms[i]);
}
void average(ProfileSummary& summary, std::size_t intervals) {
  auto& mean = summary.mean;
  const double frames = static_cast<double>(summary.samples);
  mean.frame_ms /= frames;
  for (auto& phase : mean.phase_ms) phase /= frames;
  mean.has_interval = summary.peak.has_interval = intervals != 0;
  if (intervals != 0) mean.interval_ms /= static_cast<double>(intervals);
  if (mean.interval_ms > 0.0) summary.frames_per_second = kMillisecondsPerSecond / mean.interval_ms;
}
}  // namespace

std::string_view profile_phase_name(ProfilePhase phase) {
  const auto index = static_cast<std::size_t>(phase);
  return index < kPhaseNames.size() ? kPhaseNames[index] : std::string_view{};
}

FrameProfiler::FrameProfiler(Stopwatch::Reading reading)
    : reading_(reading ? std::move(reading) : Stopwatch::Reading(steady_seconds)) {}

void FrameProfiler::invalidate_frame() {
  frame_open_ = false;
  previous_end_.reset();
  ++epoch_;
}
void FrameProfiler::clear() {
  next_ = count_ = 0;
  current_ = {};
  invalidate_frame();
}
void FrameProfiler::set_enabled(bool enabled) {
  if (enabled_ == enabled) return;
  enabled_ = enabled;
  if (enabled_)
    clear();
  else
    invalidate_frame();
}
void FrameProfiler::begin_frame() {
  if (!enabled_) return;
  ++epoch_;
  const double now = reading_();
  frame_open_ = std::isfinite(now);
  current_ = {};
  if (!frame_open_) return invalidate_frame();
  frame_start_ = now;
}
void FrameProfiler::finish_frame() {
  if (!collecting()) return;
  const double now = reading_();
  if (!std::isfinite(now)) return invalidate_frame();
  current_.frame_ms = milliseconds_between(frame_start_, now);
  current_.has_interval = previous_end_.has_value();
  if (previous_end_) current_.interval_ms = milliseconds_between(*previous_end_, now);
  previous_end_ = now;
  history_[next_] = current_;
  next_ = (next_ + 1) % kProfileHistoryFrames;
  count_ = std::min(count_ + 1, kProfileHistoryFrames);
  frame_open_ = false;
}
FrameProfiler::Mark FrameProfiler::mark() const {
  if (!collecting()) return {};
  return {epoch_, reading_()};
}
void FrameProfiler::finish_scope(ProfilePhase phase, const Mark& mark) {
  const auto index = static_cast<std::size_t>(phase);
  if (!accepts(mark)) return;
  if (index >= kProfilePhaseCount) return;
  current_.phase_ms[index] += milliseconds_between(mark.seconds, reading_());
}
ProfileSummary FrameProfiler::summary() const {
  ProfileSummary summary;
  summary.samples = count_;
  if (count_ == 0) return summary;
  summary.latest = history_[(next_ + kProfileHistoryFrames - 1) % kProfileHistoryFrames];
  std::size_t intervals = 0;
  for (std::size_t i = 0; i < count_; ++i) {
    add(summary.mean, history_[i]);
    keep_peak(summary.peak, history_[i]);
    intervals += history_[i].has_interval ? 1 : 0;
  }
  average(summary, intervals);
  return summary;
}

FrameProfileScope::FrameProfileScope(FrameProfiler& profiler, ProfilePhase phase)
    : profiler_(profiler), phase_(phase), mark_(profiler.mark()) {}
FrameProfileScope::~FrameProfileScope() { profiler_.finish_scope(phase_, mark_); }

}  // namespace redgem
