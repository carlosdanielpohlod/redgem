#include "core/folder_names.hpp"

#include <algorithm>
#include <iterator>
#include <optional>

namespace redgem {
namespace {

// What Windows or the Mac forbid in a name: the separators, the drive mark and the wildcards.
constexpr std::string_view kForbiddenMarks = "<>:\"/\\|?*";
// What is taken off both ends of a name.
constexpr std::string_view kTrimmed = " .";
constexpr char kExtensionMark = '.';
constexpr char kSpace = ' ';
// Room kept for the '_' a device's name is given.
constexpr std::size_t kDeviceMarkBytes = 1;
// The names Windows keeps for its devices, whatever their case and whatever extension follows.
// (COM and LPT with a superscript digit too: "\xC2\xB9" is U+00B9, superscript one, in UTF-8.)
constexpr std::string_view kDeviceNames[] = {
    "CON",         "PRN",         "AUX",         "NUL",  "CONIN$", "CONOUT$",     "COM0",        "COM1",
    "COM2",        "COM3",        "COM4",        "COM5", "COM6",   "COM7",        "COM8",        "COM9",
    "COM\xC2\xB9", "COM\xC2\xB2", "COM\xC2\xB3", "LPT0", "LPT1",   "LPT2",        "LPT3",        "LPT4",
    "LPT5",        "LPT6",        "LPT7",        "LPT8", "LPT9",   "LPT\xC2\xB9", "LPT\xC2\xB2", "LPT\xC2\xB3"};

// The characters no name holds: control characters, both sets (U+0000 to U+001F, U+007F to U+009F).
constexpr char32_t kFirstPrintable = 0x20;
constexpr char32_t kDelete = 0x7F;
constexpr char32_t kLastSecondSetControl = 0x9F;
constexpr char32_t kFirstNonAscii = 0x80;

// UTF-8: what the first byte of a character says of its length, and the bits of it that are the
// character's.
struct LeadByte {
  unsigned char mask;
  unsigned char value;
  std::size_t length;
  char32_t smallest;  // a smaller character written this long is written too long: not UTF-8
};
constexpr LeadByte kLeadBytes[] = {
    {0x80, 0x00, 1, 0x0}, {0xE0, 0xC0, 2, 0x80}, {0xF0, 0xE0, 3, 0x800}, {0xF8, 0xF0, 4, 0x10000}};
constexpr unsigned char kContinuationMask = 0xC0;
constexpr unsigned char kContinuationByte = 0x80;
constexpr unsigned char kContinuationBits = 0x3F;
constexpr int kBitsPerContinuation = 6;
constexpr char32_t kLargestCharacter = 0x10FFFF;
constexpr char32_t kFirstSurrogate = 0xD800;  // halves of UTF-16's pairs: no character of their own
constexpr char32_t kLastSurrogate = 0xDFFF;

struct Letter {
  char32_t code;
  std::size_t length;  // in bytes
};

const LeadByte* lead_of(unsigned char byte) {
  for (const LeadByte& lead : kLeadBytes) {
    if ((byte & lead.mask) == lead.value) return &lead;
  }
  return nullptr;
}

bool is_continuation(unsigned char byte) { return (byte & kContinuationMask) == kContinuationByte; }

bool is_surrogate(char32_t code) { return code >= kFirstSurrogate && code <= kLastSurrogate; }

// Whether a character read from `lead.length` bytes is one UTF-8 writes that way.
bool well_formed(char32_t code, const LeadByte& lead) {
  return code >= lead.smallest && code <= kLargestCharacter && !is_surrogate(code);
}

// Whether a character that starts at `at` with `lead` has all its bytes in the text.
bool fits(std::string_view text, std::size_t at, const LeadByte* lead) {
  return lead != nullptr && at + lead->length <= text.size();
}

// The character that starts at `at`; nullopt when the bytes there are not UTF-8.
std::optional<Letter> letter_at(std::string_view text, std::size_t at) {
  const auto first = static_cast<unsigned char>(text[at]);
  const LeadByte* lead = lead_of(first);
  if (!fits(text, at, lead)) return std::nullopt;
  char32_t code = first & static_cast<unsigned char>(~lead->mask);
  for (std::size_t i = 1; i < lead->length; ++i) {
    const auto byte = static_cast<unsigned char>(text[at + i]);
    if (!is_continuation(byte)) return std::nullopt;
    code = (code << kBitsPerContinuation) | (byte & kContinuationBits);
  }
  if (!well_formed(code, *lead)) return std::nullopt;
  return Letter{code, lead->length};
}

bool is_control(char32_t code) { return code < kFirstPrintable || (code >= kDelete && code <= kLastSecondSetControl); }

bool is_forbidden_mark(char32_t code) {
  return code < kFirstNonAscii && kForbiddenMarks.find(static_cast<char>(code)) != std::string_view::npos;
}

// Whether a character read from a name goes into its folder's name as it is.
bool kept_as_is(const std::optional<Letter>& letter) {
  return letter.has_value() && !is_control(letter->code) && !is_forbidden_mark(letter->code);
}

// The name with '_' for what no folder name holds, cut to `budget` bytes between whole letters.
std::string replaced(std::string_view name, std::size_t budget) {
  static constexpr std::string_view kStandIn{&kFolderNameStandIn, 1};
  std::string folder;
  std::size_t at = 0;
  while (at < name.size()) {
    const std::optional<Letter> letter = letter_at(name, at);
    const std::size_t length = letter ? letter->length : 1;  // (a byte that is not UTF-8 alone)
    const std::string_view kept = kept_as_is(letter) ? name.substr(at, length) : kStandIn;
    if (folder.size() + kept.size() > budget) break;
    folder += kept;
    at += length;
  }
  return folder;
}

std::string trimmed(const std::string& name) {
  const std::size_t first = name.find_first_not_of(kTrimmed);
  if (first == std::string::npos) return {};
  return name.substr(first, name.find_last_not_of(kTrimmed) - first + 1);
}

// Where the part of a name before its extension ends.
std::size_t stem_end(const std::string& name) { return std::min(name.find(kExtensionMark), name.size()); }

std::string ascii_upper(std::string_view text) {
  std::string upper(text);
  std::transform(upper.begin(), upper.end(), upper.begin(),
                 [](char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c; });
  return upper;
}

// Whether Windows takes the name for one of its devices: its part before the extension, without
// the spaces that end it, is a device's name.
bool names_device(const std::string& name) {
  std::string_view stem = std::string_view(name).substr(0, stem_end(name));
  stem = stem.substr(0, stem.find_last_not_of(kSpace) + 1);
  const std::string upper = ascii_upper(stem);
  return std::find(std::begin(kDeviceNames), std::end(kDeviceNames), upper) != std::end(kDeviceNames);
}

}  // namespace

std::string safe_folder_name(std::string_view name) {
  std::string folder = trimmed(replaced(name, kMaxFolderNameBytes - kDeviceMarkBytes));
  if (names_device(folder)) folder.insert(stem_end(folder), 1, kFolderNameStandIn);
  return folder;
}

}  // namespace redgem
