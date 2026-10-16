#include "core/safe_path.hpp"

#include <fstream>
#include <mutex>
#include <set>

#include "core/log.hpp"

namespace redgem {
namespace {

constexpr char kSeparator = '/';
constexpr std::string_view kParentStep = "..";
constexpr const char* kHere = ".";
// Characters no asset name holds: Windows' separator and drive mark, and the end-of-text byte.
constexpr std::string_view kForbiddenCharacters{"\\:\0", 3};
constexpr unsigned char kFirstPrintable = 0x20;  // space: everything below is a control character
constexpr unsigned char kDelete = 0x7F;          // the one control character above the letters
constexpr char kControlStandIn = '?';
// In UTF-8 the second set of control characters (U+0080 to U+009F, which some terminals obey as
// they do escapes) is this byte followed by one of a range.
constexpr unsigned char kSecondSetLead = 0xC2;
constexpr unsigned char kSecondSetFirst = 0x80;
constexpr unsigned char kSecondSetLast = 0x9F;
constexpr std::string_view kLineMarks = "\n\t";  // the control characters a message of several lines is made with

bool has_parent_step(std::string_view name) {
  for (;;) {
    const std::size_t cut = name.find(kSeparator);
    if (name.substr(0, cut) == kParentStep) return true;
    if (cut == std::string_view::npos) return false;
    name.remove_prefix(cut + 1);
  }
}

bool is_control(char c) {
  const auto byte = static_cast<unsigned char>(c);
  return byte < kFirstPrintable || byte == kDelete;
}

// Whether the byte at `at` is the one that starts a control character of the second set, with
// another after it.
bool leads_second_set(std::string_view text, std::size_t at) {
  return static_cast<unsigned char>(text[at]) == kSecondSetLead && at + 1 < text.size();
}

// Whether the two bytes at `at` are a control character of the second set.
bool is_second_set_control(std::string_view text, std::size_t at) {
  if (!leads_second_set(text, at)) return false;
  const auto next = static_cast<unsigned char>(text[at + 1]);
  return next >= kSecondSetFirst && next <= kSecondSetLast;
}

// A control character that is none of `kept`.
bool is_unkept_control(char c, std::string_view kept) {
  return is_control(c) && kept.find(c) == std::string_view::npos;
}

// The text with '?' for each of its control characters, but for those of `kept`.
std::string without_controls(std::string_view text, std::string_view kept) {
  std::string shown(text);
  for (std::size_t i = 0; i < shown.size(); ++i) {
    if (is_second_set_control(shown, i)) {
      shown[i] = shown[i + 1] = kControlStandIn;
    } else if (is_unkept_control(shown[i], kept)) {
      shown[i] = kControlStandIn;
    }
  }
  return shown;
}

// A name there is, which does not start at the root of the disk.
bool is_relative_name(std::string_view name) { return !name.empty() && name.front() != kSeparator; }

// Whether a path worked out from a folder leads to somewhere inside it.
bool stays_below(const std::filesystem::path& below) {
  return !below.empty() && !below.is_absolute() && *below.begin() != kParentStep;
}

// Whether a link is not yet named in the log. Each is named once: a game asks for the same file
// again and again (a sound each time it plays, a picture each time a scene loads).
bool first_time_met(const std::filesystem::path& link) {
  static std::mutex guard;  // (rooms of a server load their scenes at the same time)
  static std::set<std::filesystem::path> met;
  const std::lock_guard<std::mutex> lock(guard);
  return met.insert(link).second;
}

void warn_of_link(const std::filesystem::path& link) {
  if (!first_time_met(link)) return;
  log::warning(printable(link.string()) + " is a symbolic link: built games do not carry links, so it is left out");
}

}  // namespace

bool safe_asset_name(std::string_view name) {
  if (!is_relative_name(name)) return false;
  if (name.find_first_of(kForbiddenCharacters) != std::string_view::npos) return false;
  return !has_parent_step(name);
}

bool through_link(const std::filesystem::path& folder, const std::filesystem::path& relative) {
  // (A path given whole is looked at below the folder only: the folder's own may hold links.)
  const std::filesystem::path below = relative.is_absolute() ? relative.lexically_relative(folder) : relative;
  std::filesystem::path step = folder;
  // (Every step as written: in "linked/../file" the ".." leaves the folder the link leads to.)
  for (const std::filesystem::path& part : below) {
    step /= part;
    std::error_code fs_error;
    if (!std::filesystem::is_symlink(step, fs_error)) continue;
    warn_of_link(step);
    return true;
  }
  return false;
}

void warn_of_unsafe_asset_name(std::string_view name) {
  log::warning("the name '" + printable(name) + "' is refused: a game reaches only the files of its own folders");
}

std::optional<std::filesystem::path> file_inside(const std::filesystem::path& folder,
                                                 const std::filesystem::path& relative) {
  std::error_code fs_error;
  // (A file named without a folder is in the one the program runs in.)
  const std::filesystem::path root =
      std::filesystem::weakly_canonical(folder.empty() ? std::filesystem::path(kHere) : folder, fs_error);
  if (fs_error) return std::nullopt;
  std::filesystem::path file = std::filesystem::weakly_canonical(root / relative, fs_error);
  if (fs_error) return std::nullopt;
  if (!stays_below(file.lexically_relative(root))) return std::nullopt;
  if (!std::filesystem::is_regular_file(file, fs_error)) return std::nullopt;
  if (through_link(folder, relative)) return std::nullopt;
  return file;
}

std::optional<std::vector<std::uint8_t>> read_file_up_to(const std::filesystem::path& file, std::uintmax_t max_bytes) {
  std::error_code fs_error;
  if (!std::filesystem::is_regular_file(file, fs_error)) return std::nullopt;
  const std::uintmax_t size = std::filesystem::file_size(file, fs_error);
  if (fs_error) return std::nullopt;
  if (size > max_bytes) return std::nullopt;
  std::ifstream in(file, std::ios::binary);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  // (Exactly the size it had when asked: a file that grows meanwhile is not followed.)
  if (!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return std::nullopt;
  return bytes;
}

std::string printable(std::string_view text) { return without_controls(text, {}); }

std::string printable_lines(std::string_view text) { return without_controls(text, kLineMarks); }

}  // namespace redgem
