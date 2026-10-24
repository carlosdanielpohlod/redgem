// Crashes on Linux and the Mac: the fatal signals. What a signal handler does is limited to what
// POSIX calls async-signal-safe: the report is written with open/write into buffers made
// beforehand, nothing is allocated, and the signal is raised again at the end so that the
// program ends as it would have (and the system may still keep a core dump of it).

#if !defined(_WIN32)

#if !defined(__ANDROID__)
#include <execinfo.h>
#endif
#include <fcntl.h>
#include <pthread.h>
#if defined(__APPLE__)
#include <sys/ucontext.h>
#endif
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>

#include "app/crash_handlers.hpp"
#include "app/crash_report.hpp"
#include "app/crash_text.hpp"
#include "core/signal_stack.hpp"

namespace redgem::crash {
namespace {

// (SIGBUS too: what the Mac sends for some of the bad memory accesses Linux sends SIGSEGV for.)
constexpr std::array<int, 5> kFatalSignals = {SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL};
// How a shell tells a program the signal ended: what a crash during the report ends with.
constexpr int kSignalExitBase = 128;
constexpr int kMaxFrames = 64;
constexpr std::size_t kMaxPathBytes = 4096;
constexpr mode_t kFolderMode = 0755;
constexpr mode_t kFileMode = 0644;
constexpr char kSeparator = '/';
constexpr const char* kLineEnd = "\n";
constexpr const char* kNoFrames = "  (none: the system gave no frames)\n";

// Set once a crash is being reported, and by which thread. (A lock-free atomic may be used in a
// signal handler.)
std::atomic<bool> reporting{false};
pthread_t reporter;

void write_text(int fd, const char* text) {
  std::size_t left = std::strlen(text);
  while (left > 0) {
    const ssize_t written = write(fd, text, left);
    if (written <= 0) return;
    text += written;
    left -= static_cast<std::size_t>(written);
  }
}

using ReportPath = FixedText<kMaxPathBytes>;

// Makes every folder of a path that is not there yet.
void make_folders(ReportPath& folder) {
  char* path = folder.data();
  for (char* at = path + 1; *at != '\0'; ++at) {
    if (*at != kSeparator) continue;
    *at = '\0';
    mkdir(path, kFolderMode);
    *at = kSeparator;
  }
  mkdir(path, kFolderMode);
}

ReportPath report_path(const Moment& moment) {
  ReportPath path;
  path.append(prepared_crash().folder_text.c_str());
  path.append(kSeparator);
  path.append(kReportPrefix);
  path.append(moment_stamp(moment).c_str());
  path.append(kReportExtension);
  return path;
}

#if defined(__APPLE__)
// Where the crashed function's frame is, as the signal's context says. (The Mac's backtrace()
// stops at once on the handler's own stack, which is not the thread's.)
void* crashed_frame(const void* context) {
  const auto* machine = static_cast<const ucontext_t*>(context)->uc_mcontext;
#if defined(__aarch64__)
  return reinterpret_cast<void*>(__darwin_arm_thread_state64_get_fp(machine->__ss));
#else
  return reinterpret_cast<void*>(machine->__ss.__rbp);
#endif
}

int crashed_frames(void** frames, const void* context) {
  if (__builtin_available(macOS 12.0, *)) return backtrace_from_fp(crashed_frame(context), frames, kMaxFrames);
  return 0;
}
#elif !defined(__ANDROID__)
int crashed_frames(void** frames, const void*) { return backtrace(frames, kMaxFrames); }
#else
int crashed_frames(void**, const void*) { return 0; }  // (no backtrace() on the oldest Androids)
#endif

void write_backtrace(int fd, const void* context) {
  void* frames[kMaxFrames];
  const int count = crashed_frames(frames, context);
  if (count <= 0) write_text(fd, kNoFrames);
#if !defined(__ANDROID__)
  for (int i = 0; i < count; ++i) {
    write_text(fd, kFrameIndent);
    backtrace_symbols_fd(&frames[i], 1, fd);
  }
#endif
}

void write_signal_report(int signal, const void* context) {
  const Moment moment = utc_moment(static_cast<std::int64_t>(time(nullptr)));
  ReportPath folder;
  folder.append(prepared_crash().folder_text.c_str());
  make_folders(folder);
  const int fd = open(report_path(moment).c_str(), O_WRONLY | O_CREAT | O_TRUNC, kFileMode);
  if (fd < 0) return;
  write_text(fd, prepared_crash().head.c_str());
  write_text(fd, kTimeLabel);
  write_text(fd, moment_text(moment).c_str());
  write_text(fd, kCauseLabel);
  write_text(fd, signal_description(signal));
  write_text(fd, kBacktraceLabel);
  write_backtrace(fd, context);
  close(fd);
}

// Whether this thread is the one reporting a crash (pthread_self is not on POSIX's list of what a
// handler may call, but it reads a register on every system the engine runs on).
bool reporting_here() { return pthread_equal(reporter, pthread_self()) != 0; }

// A crash while another thread reports one waits for that report, which ends the program; one in
// the reporting thread itself (the handler crashing) ends it at once.
void wait_for_the_report(int signal) {
  if (reporting_here()) _exit(kSignalExitBase + signal);
  for (;;) pause();
}

void on_fatal_signal(int signal, siginfo_t*, void* context) {
  if (reporting.exchange(true)) wait_for_the_report(signal);
  reporter = pthread_self();
  write_signal_report(signal, context);
  write_text(STDERR_FILENO, prepared_crash().closed_message.c_str());
  write_text(STDERR_FILENO, kLineEnd);
  std::signal(signal, SIG_DFL);
  raise(signal);
}

}  // namespace

void install_fatal_handlers() {
  give_this_thread_a_signal_stack();
  (void)current_backtrace();  // the first backtrace loads what it needs: not in a handler
  struct sigaction action = {};
  action.sa_sigaction = on_fatal_signal;
  action.sa_flags = SA_ONSTACK | SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  for (const int signal : kFatalSignals) sigaction(signal, &action, nullptr);
}

std::vector<std::string> current_backtrace() {
#if defined(__ANDROID__)
  return {};
#else
  void* frames[kMaxFrames];
  const int count = backtrace(frames, kMaxFrames);
  const std::unique_ptr<char*, void (*)(void*)> names(backtrace_symbols(frames, count), std::free);
  if (!names) return {};
  return std::vector<std::string>(names.get(), names.get() + count);
#endif
}

void show_player(const std::string& message) {
  write_text(STDERR_FILENO, message.c_str());
  write_text(STDERR_FILENO, kLineEnd);
}

}  // namespace redgem::crash

#endif
