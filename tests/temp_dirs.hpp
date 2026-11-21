#pragma once

// Folders of a test's own among the temporary files. Test runs side by side (two worktrees, two CI
// jobs, ctest -j) never share one: each run works in a folder made for it alone, removed when the
// run ends, and each unique_temp_dir is a folder made for one test alone, removed when it ends.

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

namespace redgem::test {

// Set to anything, it keeps every test folder after the run, to look into.
inline constexpr const char* kKeepTestDirsVariable = "REDGEM_KEEP_TEST_DIRS";
// The run's own folder among the temporary files: redgem-tests-<random>-<count>.
inline constexpr const char* kRunFolderName = "redgem-tests";
// How many names a new folder tries before giving up (a name is only taken when nobody has it).
inline constexpr int kFreshDirTries = 64;

inline bool keep_test_dirs() {
  const char* value = std::getenv(kKeepTestDirsVariable);
  return value != nullptr && *value != '\0';
}

// A part of a folder's name nobody else picks: random, and counted within this process.
inline std::string fresh_suffix() {
  static std::atomic<unsigned long long> count{0};
  thread_local std::mt19937_64 random{std::random_device{}()};
  return std::to_string(random()) + "-" + std::to_string(++count);
}

// A new folder `<name>-<suffix>` in `parent`, made by this call: a folder already there is never taken.
inline std::filesystem::path make_fresh_dir(const std::filesystem::path& parent, const std::string& name) {
  std::filesystem::create_directories(parent);
  for (int tries = 0; tries < kFreshDirTries; ++tries) {
    std::filesystem::path dir = parent / (name + "-" + fresh_suffix());
    if (std::filesystem::create_directory(dir)) return dir;
  }
  throw std::runtime_error("no fresh test folder could be made in " + parent.string());
}

// A folder that is removed with this object, unless REDGEM_KEEP_TEST_DIRS is set.
class TempDir {
 public:
  explicit TempDir(std::filesystem::path path) : path_(std::move(path)) {}
  ~TempDir() { remove(); }
  TempDir(TempDir&& other) noexcept : path_(std::exchange(other.path_, {})) {}
  TempDir& operator=(TempDir&& other) noexcept {
    if (this != &other) {
      remove();
      path_ = std::exchange(other.path_, {});
    }
    return *this;
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  const std::filesystem::path& path() const { return path_; }
  operator const std::filesystem::path&() const { return path_; }  // NOLINT(google-explicit-constructor)
  std::filesystem::path operator/(const std::filesystem::path& part) const { return path_ / part; }

 private:
  void remove() {
    if (path_.empty() || keep_test_dirs()) return;
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  std::filesystem::path path_;
};

// This run's own folder among the temporary files, made on first use and removed when the run ends.
// A test's fixed folder name goes in it, never straight in the temporary files.
inline const std::filesystem::path& temp_root() {
  static const TempDir root(make_fresh_dir(std::filesystem::temp_directory_path(), kRunFolderName));
  return root.path();
}

// A fresh, empty folder for one test, named after `name`, removed when the returned object goes.
inline TempDir unique_temp_dir(const std::string& name) { return TempDir(make_fresh_dir(temp_root(), name)); }

}  // namespace redgem::test
