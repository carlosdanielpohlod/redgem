#include "render/gpu_timers.hpp"

#include <rlgl.h>

#include "render/gl_extras.hpp"

namespace redgem {
namespace {

constexpr std::array<std::string_view, kGpuPassCount> kPassNames{"prepare", "scene", "post"};

std::size_t index_of(GpuPass pass) { return static_cast<std::size_t>(pass); }

}  // namespace

std::string_view gpu_pass_name(GpuPass pass) {
  const std::size_t index = index_of(pass);
  return index < kPassNames.size() ? kPassNames[index] : std::string_view{};
}

std::size_t gpu_write_set(std::uint64_t frame) { return static_cast<std::size_t>(frame % kGpuTimerFrames); }

std::size_t gpu_read_set(std::uint64_t frame) {
  return static_cast<std::size_t>((frame + kGpuTimerFrames - 1) % kGpuTimerFrames);
}

double gpu_milliseconds(std::uint64_t nanoseconds) {
  return static_cast<double>(nanoseconds) / kNanosecondsPerMillisecond;
}

void GpuTimers::set_enabled(bool enabled) {
  if (enabled == enabled_) return;
  end();
  enabled_ = enabled;
  timings_ = {};
  issued_ = {};
}

// Asks the card once whether it can time, and makes the queries when it can.
bool GpuTimers::loaded() {
  if (checked_) return supported_;
  checked_ = true;
  supported_ = render_detail::has_timer_queries();
  if (!supported_) return false;
  for (QuerySet& set : queries_) {
    for (unsigned int& query : set) query = render_detail::make_timer_query();
  }
  return true;
}

bool GpuTimers::active() { return enabled_ && loaded(); }

void GpuTimers::start_frame() {
  if (!active()) return;
  end();
  ++frame_;
  collect(gpu_read_set(frame_));
}

void GpuTimers::collect(std::size_t set) {
  for (std::size_t pass = 0; pass < kGpuPassCount; ++pass) collect_pass(set, pass);
}

// An answer not there yet is left for the next frame's look (until the set is written again).
void GpuTimers::collect_pass(std::size_t set, std::size_t pass) {
  const unsigned int query = queries_[set][pass];
  if (!issued_[set][pass] || !render_detail::timer_ready(query)) return;
  timings_.pass_ms[pass] = gpu_milliseconds(render_detail::timer_nanoseconds(query));
  timings_.measured = true;
  issued_[set][pass] = false;
}

// (What raylib still holds back is sent first, so that it counts for the pass it was drawn in.)
void GpuTimers::begin(GpuPass pass) {
  if (!active()) return;
  end();
  rlDrawRenderBatchActive();
  const std::size_t set = gpu_write_set(frame_);
  render_detail::begin_timer(queries_[set][index_of(pass)]);
  issued_[set][index_of(pass)] = true;
  running_ = pass;
}

void GpuTimers::end() {
  if (!running_) return;
  rlDrawRenderBatchActive();
  render_detail::end_timer();
  running_.reset();
}

void GpuTimers::release() {
  end();
  for (QuerySet& set : queries_) {
    for (unsigned int& query : set) render_detail::release_timer_query(query);
  }
  queries_ = {};
  issued_ = {};
  checked_ = false;
  supported_ = false;
}

}  // namespace redgem
