// Crashes on purpose, for tests/integration/crash_handling_test.cpp: it installs the crash
// handling a shipped game has, for a game, then crashes the way it is asked to.
//   redgem_crash_helper <user data folder> <game folder> segv|abort|terminate|terminate-memory|exception|memory
// (Built with the tests, never shipped.)

#include <csignal>
#include <cstdlib>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>

#include "app/crash_report.hpp"
#include "app/game_texts.hpp"
#include "core/exit_codes.hpp"
#include "core/platform.hpp"

namespace {

constexpr int kArguments = 4;

// A real invalid access, through a pointer the compiler cannot see is null.
void touch_nothing() {
  volatile int* volatile nowhere = nullptr;
  *nowhere = 1;
}

// As the player's main: what comes out of the game reaches the top.
int crash(const std::string& how) {
  if (how == "segv") touch_nothing();
  if (how == "abort") std::abort();
  // An exception out of a thread: nobody can catch it, std::terminate is called.
  if (how == "terminate") std::thread([] { throw std::runtime_error("thrown where nobody catches it"); }).join();
  // Memory running out where nobody catches it.
  if (how == "terminate-memory") std::thread([] { throw std::bad_alloc(); }).join();
  if (how == "exception") throw std::runtime_error("thrown out of the game");
  if (how == "memory") throw std::bad_alloc();
  return redgem::kExitUsage;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != kArguments) return redgem::kExitUsage;
  redgem::platform::set_user_data_dir(argv[1]);
  redgem::add_game_texts();
  redgem::crash::install_crash_handling({argv[2], false});
  try {
    return crash(argv[3]);
  } catch (const std::bad_alloc&) {
    return redgem::crash::end_out_of_memory();
  } catch (const std::exception& error) {
    return redgem::crash::end_with_exception(error.what());
  }
}
