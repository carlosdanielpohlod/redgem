#include "scene/animation/clip_clock.hpp"

#include <cmath>

namespace redgem {
namespace {

// A looping clip's time kept within the clip, so it stays precise however long the game runs.
float wrapped(float time, float duration) {
  if (duration <= 0.0f) return time;
  const float within = std::fmod(time, duration);
  return within < 0.0f ? within + duration : within;
}

// A clip played once that reached the end it was heading for has nowhere left to go.
bool has_ended(const ClipClock& clock, float duration, bool loop, bool backwards) {
  if (loop) return false;
  return backwards ? clock.time <= 0.0f : clock.time >= duration;
}

// How many laps a looping clock went round going `expected` seconds from `from` to `after`.
double laps_gone(double from, double after, double expected, double duration) {
  if (duration <= 0.0) return 0.0;
  return std::round((from + expected - after) / duration);
}

}  // namespace

ClockSpan advance_spanning(ClipClock& clock, float duration, float dt) {
  const double from = clock.time;
  const double expected = clock.playing ? static_cast<double>(dt) * clock.speed : 0.0;
  advance(clock, duration, dt);
  if (!clock.loop) return {from, clock.time, 0.0};
  return {from, clock.time, laps_gone(from, clock.time, expected, duration)};
}

void advance(ClipClock& clock, float duration, float dt) {
  if (!clock.playing) return;
  clock.time += dt * clock.speed;
  const bool past_end = clock.time >= duration;
  const bool left_the_clip = past_end || clock.time < 0.0f;
  if (!left_the_clip) return;
  if (clock.loop) {
    clock.time = wrapped(clock.time, duration);
    return;
  }
  clock.time = past_end ? duration : 0.0f;
  clock.playing = false;  // holds its last pose
}

void start(ClipClock& clock, float duration, bool loop, float speed, bool restart) {
  const bool backwards = speed < 0.0f;
  const bool from_its_start = restart || has_ended(clock, duration, loop, backwards) || !clock.posing;
  if (from_its_start) clock.time = backwards ? duration : 0.0f;
  clock.loop = loop;
  clock.speed = speed;
  clock.playing = true;
  clock.posing = true;
}

}  // namespace redgem
