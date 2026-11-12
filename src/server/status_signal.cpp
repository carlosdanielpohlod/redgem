#include "server/status_signal.hpp"

#include <atomic>
#include <csignal>

namespace {

// Set by the handler, on whichever of the server's threads the system runs it (a lock-free atomic
// is what a handler may touch from any thread), and taken by the lobby's between turns.
std::atomic<bool> status_asked{false};
static_assert(std::atomic<bool>::is_always_lock_free, "a signal handler may only touch a lock-free atomic");

}  // namespace

extern "C" {
static void note_status_request(int) { status_asked.store(true); }
}

namespace redgem::server {

#ifdef SIGUSR1
bool StatusSignal::supported() { return true; }

StatusSignal::StatusSignal() : previous_(nullptr) {
  status_asked.store(false);
  previous_ = std::signal(SIGUSR1, note_status_request);
}

StatusSignal::~StatusSignal() { std::signal(SIGUSR1, previous_ == SIG_ERR ? SIG_DFL : previous_); }
#else
bool StatusSignal::supported() { return false; }

StatusSignal::StatusSignal() : previous_(nullptr) { static_cast<void>(note_status_request); }

StatusSignal::~StatusSignal() = default;
#endif

bool StatusSignal::taken() { return status_asked.exchange(false); }

}  // namespace redgem::server
