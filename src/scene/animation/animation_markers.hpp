#pragma once

// The moments of a clip that scripts hear about (animation events): the markers a glTF file
// authors with its clips, and which of a clip's moments one frame of playing went past.

#include <string>
#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem {

// The custom property of a Blender action (exported as the glTF animation's extras) that lists
// its markers: "name@time" entries, time normalized 0..1, separated by commas:
//   "footstep@0.25, footstep@0.75"
inline constexpr const char* kAnimationEventsProperty = "redgem_events";
inline constexpr char kMarkerSeparator = ',';
inline constexpr char kMarkerTimeSign = '@';
// A moment of a clip, normalized: from its start to its end.
inline constexpr float kClipStartAt = 0.0f;
inline constexpr float kClipEndAt = 1.0f;

struct ParsedMarkers {
  std::vector<AnimationMarker> markers;
  std::vector<std::string> refused;  // entries that are not "name@time" with a time in 0..1
};

ParsedMarkers parse_animation_markers(const std::string& text);

// Where one playing of a clip went during a frame, in seconds of the clip: from `from` to `to`,
// counted on past the clip's end (or below its start, backwards) once for every lap a looping
// clip went round. `to` below `from` plays backwards.
struct ClipSpan {
  double from = 0.0;
  double to = 0.0;
  double duration = 0.0;
  bool loop = true;
};

// The indices of the moments (`marks`, in seconds within the clip) that `span` went past, in the
// order it met them, each once for every time it did. A span takes in where it starts and not
// where it ends, so that frame after frame each moment is met exactly once; a clip played once
// takes in its last moment too when it reaches it. In a loop, the end a lap heads for is met as
// the lap gets there (the other way round: taken in where the span ends, not where it starts),
// never as a lap begins. At most kMaxMarkerLaps laps are counted.
std::vector<std::size_t> marks_passed(const ClipSpan& span, const std::vector<double>& marks);

inline constexpr int kMaxMarkerLaps = 8;

}  // namespace redgem
