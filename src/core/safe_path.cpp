#include "core/safe_path.hpp"

#include <fstream>

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

}  // namespace

bool safe_asset_name(std::string_view name) {
  if (name.empty() || name.front() == kSeparator) return false;
  if (name.find_first_of(kForbiddenCharacters) != std::string_view::npos) return false;
  return !has_parent_step(name);
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
  const std::filesystem::path below = file.lexically_relative(root);
  if (below.empty() || below.is_absolute() || *below.begin() == kParentStep) return std::nullopt;
  if (!std::filesystem::is_regular_file(file, fs_error)) return std::nullopt;
  return file;
}

std::optional<std::vector<std::uint8_t>> read_file_up_to(const std::filesystem::path& file, std::uintmax_t max_bytes) {
  std::error_code fs_error;
  if (!std::filesystem::is_regular_file(file, fs_error)) return std::nullopt;
  const std::uintmax_t size = std::filesystem::file_size(file, fs_error);
  if (fs_error || size > max_bytes) return std::nullopt;
  std::ifstream in(file, std::ios::binary);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  // (Exactly the size it had when asked: a file that grows meanwhile is not followed.)
  if (!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return std::nullopt;
  return bytes;
}

std::string printable(std::string_view text) {
  std::string shown(text);
  for (char& c : shown) {
    if (is_control(c)) c = kControlStandIn;
  }
  return shown;
}

}  // namespace redgem
