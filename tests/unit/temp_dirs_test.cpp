#include <doctest/doctest.h>

#include <filesystem>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/platform.hpp"
#include "temp_dirs.hpp"

using namespace redgem;

namespace {

constexpr int kSideBySide = 4;    // tests asking for folders at once
constexpr int kFoldersEach = 50;  // folders each of them asks for

// Every folder one test asks for, one after another, all kept until the end.
std::vector<test::TempDir> ask_for_folders() {
  std::vector<test::TempDir> folders;
  for (int i = 0; i < kFoldersEach; ++i) folders.push_back(test::unique_temp_dir("approval"));
  return folders;
}

// The environment variable as it was, put back when the test ends.
struct KeptDirsSetting {
  std::string before = platform::environment(test::kKeepTestDirsVariable);
  explicit KeptDirsSetting(const std::string& value) {
    platform::set_environment(test::kKeepTestDirsVariable, value, true);
  }
  ~KeptDirsSetting() { platform::set_environment(test::kKeepTestDirsVariable, before, true); }
  KeptDirsSetting(const KeptDirsSetting&) = delete;
  KeptDirsSetting& operator=(const KeptDirsSetting&) = delete;
};

}  // namespace

TEST_CASE("the run works in a folder of its own among the temporary files, never in them straight") {
  const std::filesystem::path& root = test::temp_root();
  CHECK(std::filesystem::is_directory(root));
  CHECK(root.parent_path() == std::filesystem::temp_directory_path());
  CHECK(root.filename().string().rfind(test::kRunFolderName, 0) == 0);
  CHECK(&test::temp_root() == &root);
}

TEST_CASE("tests asking for a folder at the same time never get the same one") {
  const KeptDirsSetting removed("");
  std::vector<std::vector<test::TempDir>> asked(kSideBySide);
  std::vector<std::thread> tests;
  for (auto& folders : asked) tests.emplace_back([&folders] { folders = ask_for_folders(); });
  for (auto& running : tests) running.join();
  std::set<std::filesystem::path> seen;
  for (const auto& folders : asked) {
    for (const auto& folder : folders) {
      CHECK(std::filesystem::is_empty(folder.path()));
      CHECK(folder.path().parent_path() == test::temp_root());
      seen.insert(folder.path());
    }
  }
  CHECK(seen.size() == static_cast<std::size_t>(kSideBySide * kFoldersEach));
}

TEST_CASE("a test's folder is removed when the test ends, with everything in it") {
  const KeptDirsSetting removed("");
  std::filesystem::path made;
  {
    const test::TempDir folder = test::unique_temp_dir("approval");
    made = folder.path();
    std::filesystem::create_directories(folder / "game" / "scenes");
    CHECK(std::filesystem::exists(made / "game" / "scenes"));
  }
  CHECK_FALSE(std::filesystem::exists(made));
}

TEST_CASE("a folder handed on is removed once, by whoever has it last") {
  const KeptDirsSetting removed("");
  test::TempDir first = test::unique_temp_dir("moved");
  const std::filesystem::path made = first.path();
  test::TempDir second = std::move(first);
  CHECK(first.path().empty());  // NOLINT(bugprone-use-after-move): a moved folder names nothing
  CHECK(std::filesystem::exists(made));
  second = test::unique_temp_dir("other");
  CHECK_FALSE(std::filesystem::exists(made));
}

TEST_CASE("REDGEM_KEEP_TEST_DIRS keeps a test's folder after it ends, to look into") {
  const KeptDirsSetting kept("1");
  std::filesystem::path made;
  {
    made = test::unique_temp_dir("kept").path();
  }
  CHECK(std::filesystem::exists(made));
  std::filesystem::remove_all(made);
}
