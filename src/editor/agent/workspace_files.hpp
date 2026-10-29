#pragma once

// The files an agent reads and writes through the editor (fs/read_text_file, fs/write_text_file):
// those of the game's folder, and nothing outside it. A path that leaves the folder (by "..", by
// a symbolic link, or by being another folder altogether) is refused.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace redgem::agent {

// The largest file read or written for an agent: game scripts and scene text are far smaller.
inline constexpr std::uintmax_t kMaxTextFileBytes = std::uintmax_t{8} * 1024 * 1024;

// Which lines of a file to read: from `line` (counted from 1), at most `limit` of them.
struct LineRange {
  std::optional<std::uint32_t> line;
  std::optional<std::uint32_t> limit;
};

class Workspace {
 public:
  // The folder as the system names it (links followed): what the agent is told to work in.
  explicit Workspace(const std::filesystem::path& folder);
  const std::filesystem::path& folder() const { return folder_; }

  // The file `requested` names, when it is inside the folder; nullopt, with `error`, otherwise.
  // Relative paths are taken from the folder.
  std::optional<std::filesystem::path> resolve(const std::string& requested, std::string& error) const;
  // The text of (some lines of) a file inside the folder.
  std::optional<std::string> read(const std::string& requested, const LineRange& range, std::string& error) const;
  // Writes a file inside the folder (its folders made as needed), in place of what it held.
  bool write(const std::string& requested, const std::string& text, std::string& error) const;

 private:
  std::filesystem::path folder_;
};

// The lines of `text` that `range` asks for, each with its line end.
std::string lines_of(const std::string& text, const LineRange& range);

}  // namespace redgem::agent
