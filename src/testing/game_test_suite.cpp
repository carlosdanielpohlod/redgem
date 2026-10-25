#include "testing/game_test_suite.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <utility>

#include "core/exit_codes.hpp"
#include "core/files.hpp"
#include "core/log.hpp"
#include "core/message_lines.hpp"
#include "core/translations.hpp"
#include "testing/ruby_tests.hpp"
#include "testing/system_test.hpp"
#include "testing/testing_config.hpp"

namespace redgem::testing {
namespace {

namespace fs = std::filesystem;

constexpr const char* kTestsDir = "tests";
constexpr const char* kTestSuffix = "_test.rb";
constexpr const char* kSystemTestMark = "SystemTest.define";

bool ends_with(const std::string& text, const std::string& end) {
  return text.size() >= end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
}

// Keeps the first error said while it lives, and passes every message on as before.
class FirstError {
 public:
  FirstError() {
    previous_ = log::set_sink([this](log::Level level, const std::string& message) {
      if (level == log::Level::Error && !first_) first_ = message;
      previous_(level, message);
    });
  }
  ~FirstError() { log::set_sink(std::move(previous_)); }
  FirstError(const FirstError&) = delete;
  FirstError& operator=(const FirstError&) = delete;

  const std::optional<std::string>& first() const { return first_; }

 private:
  log::Sink previous_;
  std::optional<std::string> first_;
};

// A system test's folder of its own: its saves and notes are no other test's.
fs::path work_for(const fs::path& work_dir, const std::string& file) {
  return work_dir.empty() ? fs::path() : work_dir / fs::path(file).stem();
}

int run_file(const fs::path& game_dir, const std::string& file, const fs::path& work_dir, GpuChoice gpu) {
  std::string error;
  const std::string code = read_text_file(game_dir / file, error).value_or("");
  if (!is_system_test(code)) return run_ruby_tests(game_dir / file, game_dir);
  return run_system_test(
      {game_dir, game_dir / file, config::kDefaultScreenshotDir, true, gpu, work_for(work_dir, file)});
}

// Runs one file and says how it went. True when it passed.
bool passes(const fs::path& game_dir, const std::string& file, const fs::path& work_dir, GpuChoice gpu) {
  FirstError said;
  const bool passed = run_file(game_dir, file, work_dir, gpu) == kExitSuccess;
  if (passed) {
    std::cout << kPassedMark << file << "\n";
  } else {
    std::cout << kFailedMark << file << ": "
              << compact_message(said.first().value_or(tr("testing.suite.failed")), game_dir) << "\n";
  }
  return passed;
}

}  // namespace

std::vector<std::string> game_test_files(const fs::path& game_dir) {
  std::vector<std::string> files;
  std::error_code error;
  for (const auto& entry : fs::directory_iterator(game_dir / kTestsDir, error)) {
    const std::string name = entry.path().filename().string();
    if (ends_with(name, kTestSuffix)) files.push_back(std::string(kTestsDir) + "/" + name);
  }
  std::sort(files.begin(), files.end());
  return files;
}

bool is_system_test(const std::string& code) { return code.find(kSystemTestMark) != std::string::npos; }

int run_game_test_suite(const fs::path& game_dir, const fs::path& work_dir, GpuChoice gpu) {
  std::size_t passed = 0;
  const std::vector<std::string> files = game_test_files(game_dir);
  for (const std::string& file : files) passed += passes(game_dir, file, work_dir, gpu) ? 1 : 0;
  std::cout << tr("testing.suite.summary",
                  {{"passed", std::to_string(passed)}, {"failed", std::to_string(files.size() - passed)}})
            << "\n";
  return passed == files.size() ? kExitSuccess : kExitFailure;
}

}  // namespace redgem::testing
