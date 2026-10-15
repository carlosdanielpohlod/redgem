#include "server/stop_signals.hpp"

#include <csignal>

namespace {

// Set by the handler, read by the server between turns (all a signal handler may safely do).
volatile std::sig_atomic_t stop_asked = 0;

}  // namespace

extern "C" {
static void note_stop_request(int) { stop_asked = 1; }
}

namespace redgem::server {

namespace {

constexpr int kStopSignals[] = {SIGINT, SIGTERM};

}  // namespace

StopSignals::StopSignals() {
  stop_asked = 0;
  for (const int signal : kStopSignals) std::signal(signal, note_stop_request);
}

StopSignals::~StopSignals() {
  for (const int signal : kStopSignals) std::signal(signal, SIG_DFL);
}

bool StopSignals::asked() const { return stop_asked != 0; }

}  // namespace redgem::server
