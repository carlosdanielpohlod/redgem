#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Reading and writing whole files, and the paths of folders: what every part of the engine that
// touches the disk shares.
namespace redgem {

// What a file holds (one of the mounted game archive's, see game_files.hpp, or one on the disk);
// nullopt, with `error` saying so, when it cannot be read.
std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string& error);
// Replaces what a file holds. False, with `error` saying so, when it cannot be written.
bool write_text_file(const std::filesystem::path& path, const std::string& text, std::string& error);
// The same for bytes (a picture, a program).
bool write_bytes_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes, std::string& error);

// A folder path as the rest of the code expects it: no "." or "..", no trailing separator
// ("games/foo/" -> "games/foo", whose filename is the game's name).
std::filesystem::path normalized_folder(const std::filesystem::path& folder);

inline constexpr const char* kFolderOccupied = "folder already exists";

// Creates one directory exclusively (0700 on POSIX). False means it was not created.
bool make_private_directory(const std::filesystem::path& path, std::error_code& error);

// A temporary sibling folder, reserved without replacing any existing path.
class ReservedFolder {
 public:
  ReservedFolder(const std::filesystem::path& parent, std::string_view prefix, std::string& error,
                 std::string_view seed = {});
  ~ReservedFolder();
  ReservedFolder(const ReservedFolder&) = delete;
  ReservedFolder& operator=(const ReservedFolder&) = delete;
  const std::filesystem::path& path() const { return path_; }
  void keep() { path_.clear(); }  // leave a folder for its owner to reclaim later

 private:
  std::filesystem::path path_;
};

}  // namespace redgem
