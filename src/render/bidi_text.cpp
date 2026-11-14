#include "render/bidi_text.hpp"

#include <SheenBidi/SheenBidi.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>

namespace redgem {
namespace {

constexpr unsigned char kFirstNonAscii = 0x80;         // below it no letter is right-to-left
constexpr std::size_t kMaxOrderedBytes = 64u * 1024u;  // longer texts are drawn as written
constexpr SBLevel kRightToLeftBit = 1;                 // odd embedding levels run right to left
constexpr SBCodepoint kNoMirror = 0;
constexpr SBCodepoint kZeroWidthJoiner = 0x200d;  // joins emoji into one picture (a family)
constexpr SBCodepoint kFirstSkinTone = 0x1f3fb;   // emoji modifiers, which tint the emoji before them
constexpr SBCodepoint kLastSkinTone = 0x1f3ff;
constexpr SBCodepoint kFirstRegional = 0x1f1e6;  // regional indicators: two of them make a flag
constexpr SBCodepoint kLastRegional = 0x1f1ff;
// Codepoints from each limit up take one more UTF-8 byte.
constexpr SBCodepoint kLengthLimits[] = {0x80, 0x800, 0x10000};
constexpr unsigned kContinuationBits = 6;
constexpr SBCodepoint kContinuationMark = 0x80;
constexpr SBCodepoint kContinuationMask = 0x3f;
constexpr SBCodepoint kLeadBytes[] = {0x00, 0xc0, 0xe0, 0xf0};  // by the count of trailing bytes

template <typename Ref, void (*Release)(Ref)>
struct Releaser {
  void operator()(Ref ref) const { Release(ref); }
};
template <typename Ref, void (*Release)(Ref)>
using Owned = std::unique_ptr<std::remove_pointer_t<Ref>, Releaser<Ref, Release>>;
using Algorithm = Owned<SBAlgorithmRef, SBAlgorithmRelease>;
using Paragraph = Owned<SBParagraphRef, SBParagraphRelease>;
using Line = Owned<SBLineRef, SBLineRelease>;

// A letter and the marks after it (or one emoji picture), as bytes [begin, end) of the text; kept
// whole when a right-to-left run is reversed.
struct Cluster {
  SBUInteger begin = 0;
  SBUInteger end = 0;
  bool joining = false;    // it ends in a zero width joiner: the next codepoint belongs to it
  bool half_flag = false;  // it is one regional indicator: a second one completes the flag
};

bool plain_ascii(const std::string& text) {
  return std::all_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) < kFirstNonAscii; });
}
bool unordered(const std::string& text) { return plain_ascii(text) || text.size() > kMaxOrderedBytes; }
bool right_to_left(const SBRun& run) { return (run.level & kRightToLeftBit) != 0; }
bool is_mark(SBCodepoint codepoint) { return SBGeneralCategoryIsMark(SBCodepointGetGeneralCategory(codepoint)); }
bool in_range(SBCodepoint codepoint, SBCodepoint first, SBCodepoint last) {
  return codepoint >= first && codepoint <= last;
}
bool is_regional(SBCodepoint codepoint) { return in_range(codepoint, kFirstRegional, kLastRegional); }
bool continues_letter(SBCodepoint codepoint) {
  return is_mark(codepoint) || codepoint == kZeroWidthJoiner || in_range(codepoint, kFirstSkinTone, kLastSkinTone);
}
bool completes(const Cluster& last, SBCodepoint codepoint) {
  return last.joining || (last.half_flag && is_regional(codepoint));
}
bool attaches(const std::vector<Cluster>& clusters, SBCodepoint codepoint) {
  return !clusters.empty() && (continues_letter(codepoint) || completes(clusters.back(), codepoint));
}
void note_end(Cluster& cluster, SBCodepoint codepoint, bool started) {
  cluster.joining = codepoint == kZeroWidthJoiner;
  cluster.half_flag = started && is_regional(codepoint);
}
bool empty_paragraph(const Paragraph& paragraph) { return !paragraph || SBParagraphGetLength(paragraph.get()) == 0; }
SBCodepoint next_codepoint(const std::string& text, SBUInteger& index) {
  return SBCodepointDecodeNextFromUTF8(reinterpret_cast<const SBUInt8*>(text.data()), text.size(), &index);
}

unsigned trailing_bytes(SBCodepoint codepoint) {
  return static_cast<unsigned>(std::count_if(std::begin(kLengthLimits), std::end(kLengthLimits),
                                             [codepoint](SBCodepoint limit) { return codepoint >= limit; }));
}
void append_utf8(std::string& out, SBCodepoint codepoint) {
  const unsigned trailing = trailing_bytes(codepoint);
  out += static_cast<char>(kLeadBytes[trailing] | (codepoint >> (kContinuationBits * trailing)));
  for (unsigned i = trailing; i-- > 0;)
    out += static_cast<char>(kContinuationMark | ((codepoint >> (kContinuationBits * i)) & kContinuationMask));
}

std::vector<Cluster> clusters_of(const std::string& text, const SBRun& run) {
  std::vector<Cluster> clusters;
  SBUInteger index = run.offset;
  while (index < run.offset + run.length) {
    const SBUInteger start = index;
    const SBCodepoint codepoint = next_codepoint(text, index);
    const bool started = !attaches(clusters, codepoint);
    if (started)
      clusters.push_back({start, index, false, false});
    else
      clusters.back().end = index;
    note_end(clusters.back(), codepoint, started);
  }
  return clusters;
}

// The cluster with its letter mirrored when it has a mirror image ("(" for ")").
void append_mirrored(std::string& out, const std::string& text, const Cluster& cluster) {
  SBUInteger index = cluster.begin;
  const SBCodepoint mirror = SBCodepointGetMirror(next_codepoint(text, index));
  if (mirror == kNoMirror)
    out.append(text, cluster.begin, index - cluster.begin);
  else
    append_utf8(out, mirror);
  out.append(text, index, cluster.end - index);
}

void append_run(std::string& out, const std::string& text, const SBRun& run) {
  if (!right_to_left(run)) {
    out.append(text, run.offset, run.length);
    return;
  }
  const std::vector<Cluster> clusters = clusters_of(text, run);
  for (auto cluster = clusters.rbegin(); cluster != clusters.rend(); ++cluster) append_mirrored(out, text, *cluster);
}

// One line's letters, its paragraph separator (a line break) left where it is.
void append_line(std::string& out, const std::string& text, SBParagraphRef paragraph, SBUInteger length) {
  const SBUInteger offset = SBParagraphGetOffset(paragraph);
  const Line line(SBParagraphCreateLine(paragraph, offset, length));
  if (!line) {
    out.append(text, offset, length);
    return;
  }
  const SBRun* runs = SBLineGetRunsPtr(line.get());
  for (SBUInteger i = 0; i < SBLineGetRunCount(line.get()); ++i) append_run(out, text, runs[i]);
}

// Orders the paragraph starting at `offset`; returns how many bytes it took, 0 when it could not.
SBUInteger append_paragraph(std::string& out, const std::string& text, SBAlgorithmRef algorithm, SBUInteger offset) {
  SBUInteger length = 0;
  SBUInteger separator = 0;
  SBAlgorithmGetParagraphBoundary(algorithm, offset, text.size() - offset, &length, &separator);
  const Paragraph paragraph(SBAlgorithmCreateParagraph(algorithm, offset, length, SBLevelDefaultLTR));
  if (empty_paragraph(paragraph)) return 0;
  append_line(out, text, paragraph.get(), length - separator);
  out.append(text, offset + length - separator, separator);
  return length;
}

}  // namespace

std::string visual_order(const std::string& text) {
  if (unordered(text)) return text;
  const SBCodepointSequence sequence{SBStringEncodingUTF8, text.data(), text.size()};
  const Algorithm algorithm(SBAlgorithmCreate(&sequence));
  if (!algorithm) return text;
  std::string out;
  for (SBUInteger offset = 0; offset < text.size();) {
    const SBUInteger taken = append_paragraph(out, text, algorithm.get(), offset);
    if (taken == 0) return text;
    offset += taken;
  }
  return out;
}

}  // namespace redgem
