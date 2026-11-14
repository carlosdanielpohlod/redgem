#pragma once

// How long the graphics card spends on each pass of a frame, for the statistics overlay. Measured
// with timer queries, a few sets of them taking turns: a frame's answers are looked for while the
// frames after it are drawn (the card may run frames behind), and read only once the card has
// them, so the program never waits for it.
// Where the card cannot time (OpenGL ES, or a driver without timer queries) nothing is measured.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace redgem {

// The passes of a frame: what is prepared before the picture (shadows, reflections, the frame's
// texels), the lit scene, and post-processing.
enum class GpuPass { Prepare, Scene, Post, Count };
inline constexpr std::size_t kGpuPassCount = static_cast<std::size_t>(GpuPass::Count);
// Sets of queries that take turns: one written by this frame, the others looked at for the answers
// of the frames before (as many frames as a driver usually runs behind, and one more).
inline constexpr std::size_t kGpuTimerFrames = 4;
inline constexpr double kNanosecondsPerMillisecond = 1.0e6;

std::string_view gpu_pass_name(GpuPass pass);

// The last answers the card gave: measured is false until it has given one (or when it cannot).
struct GpuTimings {
  bool measured = false;
  std::array<double, kGpuPassCount> pass_ms{};
};

// The set of queries frame `frame` writes, and the one frame `frame - age` wrote (age below
// kGpuTimerFrames): what it reads back, oldest first.
std::size_t gpu_write_set(std::uint64_t frame);
std::size_t gpu_set_before(std::uint64_t frame, std::size_t age);
double gpu_milliseconds(std::uint64_t nanoseconds);

// Needs the window's graphics context for everything but timings(); off until enabled.
class GpuTimers {
 public:
  GpuTimers() = default;
  GpuTimers(const GpuTimers&) = delete;
  GpuTimers& operator=(const GpuTimers&) = delete;

  void set_enabled(bool enabled);
  bool enabled() const { return enabled_; }
  // A new frame: what the card answered of the frame before is read, and the sets change turns.
  void start_frame();
  // Ends the pass being timed, if any, and times `pass` from here.
  void begin(GpuPass pass);
  void end();
  // Lets the queries go (before the graphics context is).
  void release();
  const GpuTimings& timings() const { return timings_; }

 private:
  using QuerySet = std::array<unsigned int, kGpuPassCount>;
  using IssuedSet = std::array<bool, kGpuPassCount>;
  bool active();
  bool loaded();
  void collect(std::size_t set);
  void collect_pass(std::size_t set, std::size_t pass);
  // Whether the pass was timed in that set and the card has its answer.
  bool answered(std::size_t set, std::size_t pass) const;

  std::array<QuerySet, kGpuTimerFrames> queries_{};
  std::array<IssuedSet, kGpuTimerFrames> issued_{};
  std::optional<GpuPass> running_;
  std::uint64_t frame_ = 0;
  GpuTimings timings_;
  bool enabled_ = false;
  bool checked_ = false;    // whether the card was asked if it can time
  bool supported_ = false;  // its answer; the queries are made with it
};

}  // namespace redgem
