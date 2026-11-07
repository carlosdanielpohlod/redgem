// staging::write_staged on Linux, the Mac and Android: each write in a folder of its own, which no
// other user can reach into, held open, and the file moved out of it by descriptor. Whatever is
// done to names in the game's folder meanwhile (a link put where the write's folder was), the file
// moved is the one written.
#ifndef _WIN32

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <string>
#include <system_error>
#include <utility>

#include "storage/staged_write.hpp"

namespace redgem::durable::staging {
namespace {

constexpr const char* kStagedName = "file";  // the one file in a write's own folder
constexpr mode_t kOwnerOnly = 0700;          // a write's own folder: its user's alone
constexpr mode_t kOthersBits = 0077;         // what the group and others may do
constexpr int kFolderFlags = O_RDONLY | O_DIRECTORY | O_CLOEXEC;
constexpr int kOwnFolderFlags = kFolderFlags | O_NOFOLLOW;
// A file made new only: never through a link, never handed to the programs the engine starts.
constexpr int kNewFileFlags = O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC;
constexpr mode_t kNewFileMode = 0666;  // less the user's umask, as fopen makes files
constexpr int kRemoveFile = 0;
constexpr const char* kWriteBytes = "wb";
constexpr const char* kNotOwnFolder = "the folder made for the write was replaced before it was opened";

// A descriptor, closed with it.
class Descriptor {
 public:
  Descriptor() = default;
  explicit Descriptor(int descriptor) : descriptor_(descriptor) {}
  ~Descriptor() {
    if (valid()) close(descriptor_);
  }
  Descriptor(const Descriptor&) = delete;
  Descriptor& operator=(const Descriptor&) = delete;
  Descriptor& operator=(Descriptor&& other) noexcept {
    std::swap(descriptor_, other.descriptor_);
    return *this;
  }
  int get() const { return descriptor_; }
  bool valid() const { return descriptor_ >= 0; }

 private:
  int descriptor_ = -1;
};

// Where one write happens: the file's folder, and the write's own folder in it, both held open.
struct Staging {
  Descriptor parent;
  Descriptor folder;
  std::string name;  // of the write's own folder, in the parent
};

std::string system_said() { return std::error_code(errno, std::generic_category()).message(); }

// Whether a folder just opened is one the write could have made: its user's, closed to others.
bool private_folder(int folder) {
  struct stat status{};
  if (fstat(folder, &status) != 0) return false;
  return status.st_uid == geteuid() && (status.st_mode & kOthersBits) == 0;
}

// Makes the write's own folder in the parent, under a name no other write has: its name, or empty
// with why.
std::string make_folder(int parent, const std::filesystem::path& file, const char* suffix, std::string& error) {
  for (int tries = 0; tries < kMostNameTries; ++tries) {
    const std::string name = staged_name(file, suffix).string();
    if (mkdirat(parent, name.c_str(), kOwnerOnly) == 0) return name;
    if (errno != EEXIST) break;
  }
  error = system_said();
  return {};
}

// Opens the file's folder and makes the write's own in it. Why it could not; empty when it could.
std::string stage(const std::filesystem::path& file, const char* suffix, Staging& staging) {
  staging.parent = Descriptor(open(folder_of(file).c_str(), kFolderFlags));
  if (!staging.parent.valid()) return system_said();
  std::string error;
  staging.name = make_folder(staging.parent.get(), file, suffix, error);
  if (staging.name.empty()) return error;
  staging.folder = Descriptor(openat(staging.parent.get(), staging.name.c_str(), kOwnFolderFlags));
  if (!staging.folder.valid()) return system_said();
  return private_folder(staging.folder.get()) ? std::string() : kNotOwnFolder;
}

// Writes the text, flushed, into a file made new in the write's own folder.
std::string write_in(const Staging& staging, std::string_view text, const Flush& flush) {
  const int descriptor = openat(staging.folder.get(), kStagedName, kNewFileFlags, kNewFileMode);
  if (descriptor < 0) return system_said();
  std::FILE* out = fdopen(descriptor, kWriteBytes);
  if (out == nullptr) {
    const std::string why = system_said();
    close(descriptor);
    return why;
  }
  errno = 0;
  return write_and_close(out, text, flush) ? std::string() : write_error().message();
}

std::string publish(const Staging& staging, const std::filesystem::path& file) {
  const std::string name = file.filename().string();
  if (renameat(staging.folder.get(), kStagedName, staging.parent.get(), name.c_str()) == 0) return {};
  return system_said();
}

// Removes what the write made and left: its file (when it was not moved) and its folder.
void unstage(const Staging& staging) {
  if (staging.folder.valid()) unlinkat(staging.folder.get(), kStagedName, kRemoveFile);
  if (!staging.name.empty()) unlinkat(staging.parent.get(), staging.name.c_str(), AT_REMOVEDIR);
}

}  // namespace

std::string write_staged(const std::filesystem::path& file, const char* suffix, std::string_view text,
                         const Flush& flush) {
  Staging staging;
  std::string error = stage(file, suffix, staging);
  if (error.empty()) error = write_in(staging, text, flush);
  if (error.empty()) error = publish(staging, file);
  unstage(staging);
  return error;
}

}  // namespace redgem::durable::staging

#endif
