// Whether text is valid UTF-8 (RFC 3629), which every text of a message is: read byte by byte,
// with nothing made and nothing thrown, since every text that arrives is looked at.

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "net/wire_codec.hpp"

namespace redgem::net::wire {

namespace {

// The bytes a sequence may start with, how many follow, and what the first of those may be (the
// others are any continuation byte). The narrower second bytes leave out what is written longer
// than it needs to be, the halves of UTF-16's pairs, and what is past U+10FFFF.
struct SequenceStart {
  unsigned char lowest;
  unsigned char highest;
  std::size_t following;
  unsigned char second_lowest;
  unsigned char second_highest;
};
constexpr SequenceStart kStarts[] = {
    {0x00, 0x7F, 0, 0x00, 0x00}, {0xC2, 0xDF, 1, 0x80, 0xBF}, {0xE0, 0xE0, 2, 0xA0, 0xBF},
    {0xE1, 0xEC, 2, 0x80, 0xBF}, {0xED, 0xED, 2, 0x80, 0x9F}, {0xEE, 0xEF, 2, 0x80, 0xBF},
    {0xF0, 0xF0, 3, 0x90, 0xBF}, {0xF1, 0xF3, 3, 0x80, 0xBF}, {0xF4, 0xF4, 3, 0x80, 0x8F}};
constexpr unsigned char kContinuationLowest = 0x80;
constexpr unsigned char kContinuationHighest = 0xBF;

bool between(unsigned char byte, unsigned char lowest, unsigned char highest) {
  return byte >= lowest && byte <= highest;
}

const SequenceStart* start_of(unsigned char lead) {
  const auto found = std::find_if(std::begin(kStarts), std::end(kStarts), [lead](const SequenceStart& start) {
    return between(lead, start.lowest, start.highest);
  });
  return found != std::end(kStarts) ? found : nullptr;
}

// Whether the byte `position` places after a sequence's start may be there.
bool fits(const SequenceStart& start, std::size_t position, unsigned char byte) {
  if (position == 1) return between(byte, start.second_lowest, start.second_highest);
  return between(byte, kContinuationLowest, kContinuationHighest);
}

unsigned char byte_at(const std::string& text, std::size_t at) { return static_cast<unsigned char>(text[at]); }

// How long the valid sequence at `at` is; 0 when there is none.
std::size_t sequence_at(const std::string& text, std::size_t at) {
  const SequenceStart* start = start_of(byte_at(text, at));
  const bool whole_there = start != nullptr && text.size() - at > start->following;  // a start, and all it needs
  if (!whole_there) return 0;
  for (std::size_t position = 1; position <= start->following; ++position) {
    if (!fits(*start, position, byte_at(text, at + position))) return 0;
  }
  return start->following + 1;
}

}  // namespace

bool valid_utf8(const std::string& text) {
  for (std::size_t at = 0; at < text.size();) {
    const std::size_t length = sequence_at(text, at);
    if (length == 0) return false;
    at += length;
  }
  return true;
}

}  // namespace redgem::net::wire
