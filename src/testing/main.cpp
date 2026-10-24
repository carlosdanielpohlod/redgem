// redgem-test: runs a game's tests. A development tool, separate from the program games ship as.

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "app/game_texts.hpp"
#include "app/user_settings.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/translations.hpp"
#include "testing/game_test_suite.hpp"
#include "testing/ruby_tests.hpp"
#include "testing/system_test.hpp"
#include "testing/test_cli.hpp"
#include "testing/testing_texts.hpp"

namespace {

using namespace redgem;

constexpr int kExitUsage = 2;

int run_tests(const testing::TestCliOptions& options) {
  if (options.mode == testing::TestMode::Help) {
    std::cout << testing::test_usage();
    return kExitSuccess;
  }
  if (options.mode == testing::TestMode::GameTests) {
    return testing::run_game_test_suite(options.game_dir, options.work_dir, gpu_choice_for_run(options.gpu));
  }
  if (options.mode == testing::TestMode::RubyTests) {
    return testing::run_ruby_tests(options.test_file, options.scripts_game);
  }
  return testing::run_system_test({options.game_dir, options.test_file, options.screenshot_dir, options.headless,
                                   gpu_choice_for_run(options.gpu), options.work_dir});
}

int run_command_line(const std::vector<std::string>& args) {
  add_game_texts();
  testing::add_testing_texts();
  translations::use_language(language_for_run());
  const testing::TestCliResult cli = testing::parse_test_cli(args);
  if (!cli.ok) {
    std::cerr << cli.error << "\n\n" << testing::test_usage();
    return kExitUsage;
  }
  return run_tests(cli.options);
}

}  // namespace

// Whatever goes wrong while running is said, and fails the run.
int main(int argc, char** argv) {
  const std::vector<std::string> args(argv + 1, argv + argc);
  try {
    return run_command_line(args);
  } catch (const std::exception& error) {
    redgem::log::error(error.what());
    return redgem::kExitFailure;
  }
}
