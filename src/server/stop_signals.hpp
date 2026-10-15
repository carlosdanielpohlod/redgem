#pragma once

// Ctrl+C (SIGINT) and a service manager's stop (SIGTERM), heard as a request to stop instead of
// ending the program on the spot: a server finishes its turn, tells its players the games are
// over, and closes its port. For as long as one of these lives; the system's own handling comes
// back after it.
namespace redgem::server {

class StopSignals {
 public:
  StopSignals();
  ~StopSignals();
  StopSignals(const StopSignals&) = delete;
  StopSignals& operator=(const StopSignals&) = delete;

  // Whether a stop was asked for since this was made.
  bool asked() const;
};

}  // namespace redgem::server
