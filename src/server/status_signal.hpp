#pragma once

// SIGUSR1 (`kill -USR1 <pid>`, `systemctl kill -s USR1 <unit>`), heard as a request for a dedicated
// server to write its status in its log (see server_status.hpp). For as long as one of these
// lives; the system's own handling comes back after it. Windows has no such signal: there it is
// never asked for.
namespace redgem::server {

// What handled a signal before (std::signal's kind of handler).
using SignalHandler = void (*)(int);

class StatusSignal {
 public:
  // Whether this system has the signal.
  static bool supported();

  StatusSignal();
  ~StatusSignal();
  StatusSignal(const StatusSignal&) = delete;
  StatusSignal& operator=(const StatusSignal&) = delete;

  // Whether the status was asked for since the last time this was asked (it is then not any more).
  bool taken();

 private:
  [[maybe_unused]] SignalHandler previous_;  // put back when this goes (never set where there is no SIGUSR1)
};

}  // namespace redgem::server
