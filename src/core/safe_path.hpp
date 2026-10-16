#pragma once

// Names and paths that come from a game's files or scripts, kept inside the folder they belong to.
// A downloaded game is not trusted: what it names must never reach a file outside its own folder.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace redgem {

// Whether a name given by a script (a sound, a picture, a scene, a script file) can
// be joined to one of the game's folders and stay inside it: not empty, not absolute, no ".." step,
// no backslash, no ':' (drive letters, alternate streams) and no NUL. Subfolders ("ui/heart") are fine.
bool safe_asset_name(std::string_view name);

// Says in the log that an asset name was refused (see safe_asset_name).
void warn_of_unsafe_asset_name(std::string_view name);

// Whether `relative` (a path inside `folder`) is reached through a symbolic link: the file itself
// or a folder on the way below `folder`, each step taken as written. A built game carries no
// links, so a game being developed refuses them too: a warning names each link, once.
bool through_link(const std::filesystem::path& folder, const std::filesystem::path& relative);

// The file a safe `name` means in `folder`: the first of `name` + each extension that is a regular
// file reached through no link (one that is, is warned of and passed over). nullopt (with a
// warning for a name that is refused) when there is none.
template <typename Extensions>
std::optional<std::filesystem::path> find_asset_file(const std::filesystem::path& folder, const std::string& name,
                                                     const Extensions& extensions) {
  if (!safe_asset_name(name)) {
    warn_of_unsafe_asset_name(name);
    return std::nullopt;
  }
  for (const char* extension : extensions) {
    std::filesystem::path candidate = folder / (name + extension);
    std::error_code fs_error;
    if (!std::filesystem::is_regular_file(candidate, fs_error)) continue;
    if (!through_link(folder, name + extension)) return candidate;
  }
  return std::nullopt;
}

// The regular file `relative` names from `folder`, when it really is inside that folder once
// ".." steps are followed, and is reached through no link (see through_link). nullopt for
// anything else (outside, missing, linked, a device, a pipe).
std::optional<std::filesystem::path> file_inside(const std::filesystem::path& folder,
                                                 const std::filesystem::path& relative);

// Everything a regular file of at most `max_bytes` holds; nullopt for a bigger one, for what is
// not a regular file (a device or a pipe never ends) and for one that cannot be read.
std::optional<std::vector<std::uint8_t>> read_file_up_to(const std::filesystem::path& file, std::uintmax_t max_bytes);

// A text that came from a file, as it can be shown in a log: control characters (line breaks,
// terminal escapes) become '?', so a name cannot fake log lines or drive the terminal.
std::string printable(std::string_view text);
// The same for a message of several lines (a script's error with its backtrace): line breaks and
// tabs stay, every other control character becomes '?'.
std::string printable_lines(std::string_view text);

}  // namespace redgem
