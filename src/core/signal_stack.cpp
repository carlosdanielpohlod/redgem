#include "core/signal_stack.hpp"

#include <cstddef>
#include <memory>

#if defined(_WIN32)
#include <windows.h>
#else
#include <csignal>
#endif

namespace redgem {
namespace {

// Enough for a crash handler: it writes a report from buffers made beforehand.
constexpr std::size_t kSignalStackBytes = 64 * 1024;

#if !defined(_WIN32)
// The thread's alternate stack, unregistered before it is freed when the thread ends.
class SignalStack {
 public:
  SignalStack() : memory_(new char[kSignalStackBytes]) {
    stack_t stack = {};
    stack.ss_sp = memory_.get();
    stack.ss_size = kSignalStackBytes;
    sigaltstack(&stack, nullptr);
  }
  ~SignalStack() {
    stack_t off = {};
    off.ss_flags = SS_DISABLE;
    sigaltstack(&off, nullptr);
  }
  SignalStack(const SignalStack&) = delete;
  SignalStack& operator=(const SignalStack&) = delete;

 private:
  std::unique_ptr<char[]> memory_;
};
#endif

}  // namespace

#if defined(_WIN32)
void give_this_thread_a_signal_stack() {
  ULONG reserved = kSignalStackBytes;
  SetThreadStackGuarantee(&reserved);
}
#else
void give_this_thread_a_signal_stack() { thread_local const SignalStack stack; }
#endif

}  // namespace redgem
