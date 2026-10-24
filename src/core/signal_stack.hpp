#pragma once

namespace redgem {

// Gives the calling thread a stack of its own for crash handlers, so that a crash that is the
// thread's stack running out can still be reported (see app/crash_report.hpp): an alternate
// signal stack on Linux, the Mac and Android, a reserve of the thread's stack on Windows. Every
// thread the engine starts calls it first; calling it again on the same thread does nothing. On
// POSIX systems the stack is unregistered, then freed, when the thread ends.
void give_this_thread_a_signal_stack();

}  // namespace redgem
