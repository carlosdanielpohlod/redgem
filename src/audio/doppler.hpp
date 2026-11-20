#pragma once

// The Doppler effect: a sound coming nearer is heard higher, one going away lower (a car passing,
// a shell flying over). The game gives the velocities of its sounds and of the listener; the pitch
// shift is the classic ratio of the speeds along the line between them.

#include "core/math.hpp"

namespace redgem {

inline constexpr float kSpeedOfSound = 343.0f;  // m/s, in air
inline constexpr float kNoDoppler = 0.0f;       // a scale that turns it off
inline constexpr float kRealDoppler = 1.0f;     // as loud as in the world
inline constexpr float kMostDoppler = 10.0f;    // a cartoon's
inline constexpr float kNoShift = 1.0f;

// What a sound's pitch is multiplied by: (c + v_listener) / (c + v_source), each speed along the
// line from the listener to the sound (the listener's toward it, the sound's away). `scale`
// multiplies both velocities (0 off, 1 real). Each speed is kept below half the speed of sound,
// so the shift stays from a third to three times.
float doppler_shift(Vec3 listener, Vec3 listener_velocity, Vec3 source, Vec3 source_velocity, float scale);

}  // namespace redgem
