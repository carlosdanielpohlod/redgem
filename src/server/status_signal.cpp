#include "server/status_signal.hpp"

#include <csignal>

namespace {

// Set by the handler, taken by the server between turns (all a signal handler may safely do).
volatile std::sig_atomic_t status_asked = 0;

}  // namespace

extern "C" {
static void note_status_request(int) { status_asked = 1; }
}

namespace redgem::server {

#ifdef SIGUSR1
bool StatusSignal::supported() { return true; }

StatusSignal::StatusSignal() {
  status_asked = 0;
  std::signal(SIGUSR1, note_status_request);
}

StatusSignal::~StatusSignal() { std::signal(SIGUSR1, SIG_DFL); }
#else
bool StatusSignal::supported() { return false; }

StatusSignal::StatusSignal() { static_cast<void>(note_status_request); }

StatusSignal::~StatusSignal() = default;
#endif

bool StatusSignal::taken() {
  const bool asked = status_asked != 0;
  status_asked = 0;
  return asked;
}

}  // namespace redgem::server
