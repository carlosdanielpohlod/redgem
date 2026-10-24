#pragma once

#include <cstddef>
#include <cstdint>

// The pieces of a crash report that are written while the program is crashing: from a signal
// handler, where only a few functions of the system may be called (no memory may be allocated,
// no lock taken, not even the C library's own clock). Everything here is plain arithmetic on
// buffers the caller owns.
namespace redgem::crash {

// A moment of time in UTC, from its seconds since 1970.
struct Moment {
  std::int64_t year = 0;
  int month = 0;  // 1 to 12
  int day = 0;    // 1 to 31
  int hour = 0;
  int minute = 0;
  int second = 0;
};
Moment utc_moment(std::int64_t seconds_since_epoch);

// A text of at most Size - 1 bytes, always ended with a zero: what does not fit is left out.
template <std::size_t Size>
class FixedText {
 public:
  void append(const char* text) {
    for (; *text != '\0'; ++text) {
      if (!has_room()) break;
      bytes_[length_++] = *text;
    }
    bytes_[length_] = '\0';
  }
  void append(char letter) {
    const char pair[] = {letter, '\0'};
    append(pair);
  }
  // A whole number, written with at least `width` digits (zeros in front).
  void append_number(std::int64_t value, int width = 1);
  // A number in hexadecimal, small letters, no prefix ("1a2b").
  void append_hex(std::uint64_t value);
  const char* c_str() const { return bytes_; }
  // (For a system call that takes a text it may change in place, as long as it puts it back.)
  char* data() { return bytes_; }
  std::size_t size() const { return length_; }

 private:
  bool has_room() const { return length_ + 1 < Size; }  // (the last byte is the ending zero's)

  char bytes_[Size] = {};
  std::size_t length_ = 0;
};

// The digits of a number that is not negative, last first, and how many there are.
inline constexpr int kMaxDigits = 20;  // of the largest 64-bit number
int reversed_digits(std::int64_t value, char (&digits)[kMaxDigits]);

template <std::size_t Size>
void FixedText<Size>::append_number(std::int64_t value, int width) {
  if (value < 0) append('-');
  char digits[kMaxDigits];
  const int count = reversed_digits(value < 0 ? -value : value, digits);
  for (int pad = count; pad < width; ++pad) append('0');
  for (int i = count - 1; i >= 0; --i) append(digits[i]);
}

template <std::size_t Size>
void FixedText<Size>::append_hex(std::uint64_t value) {
  constexpr const char* kHexDigits = "0123456789abcdef";
  constexpr int kBitsPerDigit = 4;
  constexpr std::uint64_t kDigitMask = 0xf;
  char digits[kMaxDigits];
  int count = 0;
  do {
    digits[count++] = kHexDigits[value & kDigitMask];
    value >>= kBitsPerDigit;
  } while (value > 0);
  for (int i = count - 1; i >= 0; --i) append(digits[i]);
}

// How a moment is written in a report ("2026-10-24 13:05:09 UTC") and in a file's name
// ("20261024-130509"): the one sorts as the other.
inline constexpr std::size_t kMomentTextSize = 32;
FixedText<kMomentTextSize> moment_text(const Moment& moment);
FixedText<kMomentTextSize> moment_stamp(const Moment& moment);

// What a fatal signal is, for a report ("SIGSEGV (invalid memory access)"); a generic text for a
// signal that is not one of those handled.
const char* signal_description(int signal);

}  // namespace redgem::crash
