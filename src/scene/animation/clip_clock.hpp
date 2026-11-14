#pragma once

// Where one playing of an animation clip is: its time, how fast it runs and whether it does.

namespace redgem {

struct ClipClock {
  float time = 0.0f;
  float speed = 1.0f;  // a negative speed plays backwards
  bool playing = true;
  bool loop = true;
  bool posing = true;  // false once stopped: the clip no longer touches its objects
};

// Moves a playing clock on by dt seconds, within a clip `duration` seconds long. A looping one
// wraps around; one played once stops at the end it reaches and holds that pose.
void advance(ClipClock& clock, float duration, float dt);

// Where a clock went in one advance: from `from` to `to`, both within the clip, going round
// `laps` times on the way (negative backwards; always 0 for a clip played once). Kept apart, so
// that the next span starts exactly where this one ended.
struct ClockSpan {
  double from = 0.0;
  double to = 0.0;
  double laps = 0.0;
};

// Where a span ended counted on from where it started, in the units of `lap` (a lap is the clip's
// duration in seconds, or 1 for a normalized phase).
inline double unwrapped_end(const ClockSpan& span, double lap) { return span.to + span.laps * lap; }

// advance(), saying where the clock went.
ClockSpan advance_spanning(ClipClock& clock, float duration, float dt);

// Sets a clock playing: from where it is, or from the clip's beginning (its end, played
// backwards) when `restart` asks, when it was stopped, or when a clip played once already ended.
void start(ClipClock& clock, float duration, bool loop, float speed, bool restart);

}  // namespace redgem
