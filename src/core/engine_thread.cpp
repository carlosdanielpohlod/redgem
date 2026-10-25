#include "core/engine_thread.hpp"

#include <atomic>

#include "core/signal_stack.hpp"

namespace redgem {
namespace {

std::atomic<UncaughtHandler>& uncaught_handler() {
  static std::atomic<UncaughtHandler> handler{nullptr};
  return handler;
}

}  // namespace

void set_uncaught_handler(UncaughtHandler handler) { uncaught_handler().store(handler); }

void run_engine_thread(const std::function<void()>& work) {
  give_this_thread_a_signal_stack();
  try {
    work();
  } catch (...) {
    const UncaughtHandler handler = uncaught_handler().load();
    if (handler != nullptr) handler(std::current_exception());
    throw;
  }
}

}  // namespace redgem
