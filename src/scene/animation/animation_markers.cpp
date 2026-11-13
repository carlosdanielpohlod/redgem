#include "scene/animation/animation_markers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <utility>

namespace redgem {
namespace {

constexpr const char* kSpaces = " \t\r\n";

std::string trimmed(const std::string& text) {
  const std::size_t first = text.find_first_not_of(kSpaces);
  if (first == std::string::npos) return {};
  return text.substr(first, text.find_last_not_of(kSpaces) - first + 1);
}

bool within_the_clip(float at) { return std::isfinite(at) && at >= kClipStartAt && at <= kClipEndAt; }

// The whole of `text` read as a number, or nullopt.
std::optional<float> number_in(const std::string& text) {
  if (text.empty()) return std::nullopt;
  char* end = nullptr;
  const float value = std::strtof(text.c_str(), &end);
  if (end != text.c_str() + text.size()) return std::nullopt;
  return value;
}

bool names_a_moment(const std::string& name, const std::optional<float>& at) {
  return !name.empty() && at.has_value() && within_the_clip(*at);
}

std::optional<AnimationMarker> marker_in(const std::string& entry) {
  const std::size_t sign = entry.rfind(kMarkerTimeSign);
  if (sign == std::string::npos) return std::nullopt;
  const std::string name = trimmed(entry.substr(0, sign));
  const std::optional<float> at = number_in(trimmed(entry.substr(sign + 1)));
  if (!names_a_moment(name, at)) return std::nullopt;
  return AnimationMarker{name, *at};
}

void read_entry(const std::string& entry, ParsedMarkers& parsed) {
  const std::string text = trimmed(entry);
  if (text.empty()) return;
  if (const auto marker = marker_in(text)) {
    parsed.markers.push_back(*marker);
  } else {
    parsed.refused.push_back(text);
  }
}

// --- Which moments a frame went past ---------------------------------------------------------

struct Hit {
  double position = 0.0;  // seconds, counted on as the span is
  std::size_t mark = 0;
};

bool goes_forward(const ClipSpan& span) { return span.to >= span.from; }

// A clip played once that reached the end it was heading for: its last moment is met too.
bool reaches_its_end(const ClipSpan& span) {
  if (span.loop || span.from == span.to) return false;
  return goes_forward(span) ? span.to >= span.duration : span.to <= 0.0;
}

// From `start` (taken in) towards `end` (left out), whichever way that is.
bool on_the_way(double start, double position, double end) {
  return start <= end ? start <= position && position < end : end < position && position <= start;
}

// A looping clip's moment at the end its laps head for: the start of the next lap too.
bool ends_a_lap(const ClipSpan& span, double mark) {
  return span.loop && mark == (goes_forward(span) ? span.duration : 0.0);
}

bool passes(const ClipSpan& span, double mark, double position) {
  if (ends_a_lap(span, mark)) return on_the_way(span.to, position, span.from);
  if (position == span.to) return reaches_its_end(span);
  return on_the_way(span.from, position, span.to);
}

// No more than kMaxMarkerLaps laps of a clip, the last ones played.
ClipSpan bounded(ClipSpan span) {
  const double most = static_cast<double>(kMaxMarkerLaps) * span.duration;
  if (span.to - span.from > most) span.from = span.to - most;
  if (span.from - span.to > most) span.from = span.to + most;
  return span;
}

// The laps of a looping clip in which `mark` might be passed (a clip played once has one).
std::pair<long, long> laps_around(const ClipSpan& span, double mark) {
  if (!span.loop || span.duration <= 0.0) return {0, 0};
  const double low = std::min(span.from, span.to) - mark;
  const double high = std::max(span.from, span.to) - mark;
  return {static_cast<long>(std::floor(low / span.duration)), static_cast<long>(std::ceil(high / span.duration))};
}

void collect(const ClipSpan& span, double mark, std::size_t index, std::vector<Hit>& hits) {
  const auto [first, last] = laps_around(span, mark);
  for (long lap = first; lap <= last; ++lap) {
    const double position = mark + static_cast<double>(lap) * span.duration;
    if (passes(span, mark, position)) hits.push_back({position, index});
  }
}

}  // namespace

ParsedMarkers parse_animation_markers(const std::string& text) {
  ParsedMarkers parsed;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t comma = std::min(text.find(kMarkerSeparator, start), text.size());
    read_entry(text.substr(start, comma - start), parsed);
    start = comma + 1;
  }
  return parsed;
}

std::vector<std::size_t> marks_passed(const ClipSpan& span, const std::vector<double>& marks) {
  const ClipSpan kept = bounded(span);
  std::vector<Hit> hits;
  for (std::size_t i = 0; i < marks.size(); ++i) collect(kept, marks[i], i, hits);
  const bool forward = goes_forward(kept);
  std::stable_sort(hits.begin(), hits.end(), [forward](const Hit& a, const Hit& b) {
    return forward ? a.position < b.position : a.position > b.position;
  });
  std::vector<std::size_t> order;
  for (const Hit& hit : hits) order.push_back(hit.mark);
  return order;
}

}  // namespace redgem
