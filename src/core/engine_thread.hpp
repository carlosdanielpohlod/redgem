#pragma once

#include <exception>
#include <functional>

namespace redgem {

// What every thread the engine starts (the scripts' watchdog, web requests, a server's rooms)
// runs its work in: the thread gets a stack for crash handlers (signal_stack.hpp), and an
// exception that leaves its work goes to the program's handler for them, where it has one (a
// shipped game's crash report: app/crash_report.hpp) instead of straight to std::terminate,
// which on some systems no longer knows what it was.
void run_engine_thread(const std::function<void()>& work);

// What is called with an exception that left a thread's work; it does not return (it ends the
// program). Without one, the exception goes on to std::terminate.
using UncaughtHandler = void (*)(const std::exception_ptr&);
void set_uncaught_handler(UncaughtHandler handler);

}  // namespace redgem
