#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "core/stopwatch.hpp"

namespace redgem {

enum class ProfilePhase { Input, Network, Http, Scripts, Physics, Presentation, Scene, Count };
inline constexpr std::size_t kProfilePhaseCount = static_cast<std::size_t>(ProfilePhase::Count);
inline constexpr std::size_t kProfileHistoryFrames = 120;  // two seconds at sixty frames per second

std::string_view profile_phase_name(ProfilePhase phase);

struct ProfileFrame {
  double frame_ms = 0.0;
  double interval_ms = 0.0;   // between completed frames: includes time the caller waited outside the frame
  bool has_interval = false;  // the first frame has no preceding frame to measure
  std::array<double, kProfilePhaseCount> phase_ms{};
};

struct ProfileSummary {
  std::size_t samples = 0;
  double frames_per_second = 0.0;
  ProfileFrame latest;
  ProfileFrame mean;
  ProfileFrame peak;
};

class FrameProfileScope;

// Foreground wall-clock timings, independent of simulation time. Phases may nest (inclusive
// timings); their sum need not equal frame_ms. It measures no GPU work outside the caller's wait.
// Collection is off initially; disabled collection never reads the clock. Storage is bounded and
// no frame or scope allocates. Enabling starts a fresh window; disabling retains its last data.
class FrameProfiler {
 public:
  explicit FrameProfiler(Stopwatch::Reading reading = {});
  FrameProfiler(const FrameProfiler&) = delete;
  FrameProfiler& operator=(const FrameProfiler&) = delete;
  void set_enabled(bool enabled);
  bool enabled() const { return enabled_; }
  void clear();
  void begin_frame();
  void finish_frame();
  ProfileSummary summary() const;

 private:
  friend class FrameProfileScope;
  struct Mark {
    std::uint64_t epoch = 0;
    double seconds = 0.0;
  };
  bool collecting() const { return enabled_ && frame_open_; }
  bool accepts(const Mark& mark) const { return collecting() && mark.epoch == epoch_; }
  Mark mark() const;
  void finish_scope(ProfilePhase phase, const Mark& mark);
  void invalidate_frame();
  Stopwatch::Reading reading_;
  std::array<ProfileFrame, kProfileHistoryFrames> history_{};
  ProfileFrame current_;
  std::optional<double> previous_end_;
  double frame_start_ = 0.0;
  std::size_t next_ = 0;
  std::size_t count_ = 0;
  std::uint64_t epoch_ = 0;
  bool enabled_ = false;
  bool frame_open_ = false;
};

// The profiler outlives a scope. A cleared, disabled or finished frame discards unfinished scopes.
class FrameProfileScope {
 public:
  FrameProfileScope(FrameProfiler& profiler, ProfilePhase phase);
  ~FrameProfileScope();
  FrameProfileScope(const FrameProfileScope&) = delete;
  FrameProfileScope& operator=(const FrameProfileScope&) = delete;

 private:
  FrameProfiler& profiler_;
  ProfilePhase phase_;
  FrameProfiler::Mark mark_;
};

}  // namespace redgem
