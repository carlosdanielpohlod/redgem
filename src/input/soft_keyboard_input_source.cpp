#include "input/soft_keyboard_input_source.hpp"

#include <raylib.h>

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

void append_utf8(std::string& text, char32_t letter) {
  int bytes = 0;
  const char* encoded = CodepointToUTF8(static_cast<int>(letter), &bytes);
  text.append(encoded, static_cast<std::size_t>(bytes));
}

}  // namespace

std::string SoftKeyboardInputSource::typed_text() const {
  std::string text = devices_.typed_text() + frame_text_;
  frame_text_.clear();
  return text;
}

void SoftKeyboardInputSource::request_text_input(const TextInputRequest& request) {
  platform_.begin_frame();
  keyboard_.follow(request);
  const bool key_free = !this_frame_.has_value();  // the frame before took no key
  this_frame_.reset();
  hand_out_letters();
  if (!key_due(key_free)) return;
  this_frame_ = typed_.front().key;  // (after the letters, what is left starts with a key)
  typed_.pop_front();
}

void SoftKeyboardInputSource::hand_out_letters() {
  for (; letter_next(); typed_.pop_front()) append_utf8(frame_text_, typed_.front().letter);
}

void SoftKeyboardInputSource::type(char32_t letter) {
  if (is_typed_letter(letter)) typed_.push_back({letter, std::nullopt});
}

void SoftKeyboardInputSource::press(Key key) {
  held_.insert(key);
  typed_.push_back({0, key});
}

void SoftKeyboardInputSource::hit(Key key) {
  press(key);
  release(key);
}

}  // namespace redgem
