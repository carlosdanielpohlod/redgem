#include "input/soft_keyboard_input_source.hpp"

#include <raylib.h>

#include <utility>

namespace redgem {

namespace {

constexpr char32_t kFirstPrintable = 0x20;     // what comes before it (C0) is control characters
constexpr char32_t kFirstUpperControl = 0x7F;  // DEL, then the C1 control characters
constexpr char32_t kLastUpperControl = 0x9F;
constexpr char32_t kFirstSurrogate = 0xD800;  // halves of UTF-16 pairs: no letters of their own
constexpr char32_t kLastSurrogate = 0xDFFF;
constexpr char32_t kLastCodePoint = 0x10FFFF;

bool is_control(char32_t code) {
  return code < kFirstPrintable || (code >= kFirstUpperControl && code <= kLastUpperControl);
}

bool is_surrogate(char32_t code) { return code >= kFirstSurrogate && code <= kLastSurrogate; }

// Whether a code point is a letter (or a sign, a digit, a space) that a text can take.
bool is_typed_letter(char32_t code) { return code <= kLastCodePoint && !is_control(code) && !is_surrogate(code); }

}  // namespace

std::string SoftKeyboardInputSource::typed_text() const {
  std::string text = devices_.typed_text() + typed_;
  typed_.clear();
  return text;
}

void SoftKeyboardInputSource::request_text_input(const TextInputRequest& request) {
  platform_.begin_frame();
  keyboard_.follow(request);
  this_frame_ = std::move(since_frame_);
  since_frame_.clear();
}

void SoftKeyboardInputSource::type(char32_t letter) {
  if (!is_typed_letter(letter)) return;
  int bytes = 0;
  const char* encoded = CodepointToUTF8(static_cast<int>(letter), &bytes);
  typed_.append(encoded, static_cast<std::size_t>(bytes));
}

void SoftKeyboardInputSource::press(Key key) {
  held_.insert(key);
  since_frame_.insert(key);
}

void SoftKeyboardInputSource::hit(Key key) {
  press(key);
  release(key);
}

}  // namespace redgem
