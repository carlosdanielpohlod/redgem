#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "core/log.hpp"
#include "core/platform.hpp"

using namespace redgem;

namespace {

constexpr const char* kSettingsVariable = "XDG_CONFIG_HOME";

// Sets an environment variable for the life of the object, and puts back what it held.
class ScopedVariable {
 public:
  ScopedVariable(const char* name, const std::string& value) : name_(name) {
    const char* before = std::getenv(name);
    if (before != nullptr) before_ = before;
    platform::set_environment(name, value, true);
  }
  ~ScopedVariable() { platform::set_environment(name_, before_, true); }
  ScopedVariable(const ScopedVariable&) = delete;
  ScopedVariable& operator=(const ScopedVariable&) = delete;

 private:
  const char* name_;
  std::string before_;
};

}  // namespace

TEST_CASE("the user's data is in the desktop's settings folder, or in .config at home") {
  {
    const ScopedVariable settings(kSettingsVariable, "/somewhere/settings");
    CHECK(platform::user_data_dir() == std::filesystem::path("/somewhere/settings") / "redgem");
  }
  const ScopedVariable no_settings(kSettingsVariable, "");
  CHECK(platform::user_data_dir() == platform::home_dir() / ".config" / "redgem");
}

TEST_CASE("a system that gives the program a place of its own says where the user's data is") {
  const std::filesystem::path usual = platform::user_data_dir();
  platform::set_user_data_dir("/data/app/files");
  CHECK(platform::user_data_dir() == "/data/app/files");
  platform::set_user_data_dir({});
  CHECK(platform::user_data_dir() == usual);
}

TEST_CASE("an executable's file is named as its system names programs") {
  CHECK(platform::executable_file_name("game", false) == "game");
  CHECK(platform::executable_file_name("game", true) == "game.exe");
}

TEST_CASE("the platform says what this system is, for what is built and run differently on it") {
  // (These tests run on a desk with a shell: the Android SDK's tools can be run from it.)
  CHECK(platform::has_posix_shell() != platform::is_windows());
}

TEST_CASE("messages nobody takes are shown by the platform, and taken ones are not") {
  int taken = 0;
  log::Sink previous = log::set_sink([&taken](log::Level, const std::string&) { ++taken; });
  log::info("for the sink");
  log::set_sink(std::move(previous));
  CHECK(taken == 1);
  platform::write_log(log::Level::Debug, "a line from the platform's own log, for whoever reads the test's output");
  CHECK(taken == 1);
}
