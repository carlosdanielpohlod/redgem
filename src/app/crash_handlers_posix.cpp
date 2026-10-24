// Crashes on Linux and the Mac: the fatal signals. What a signal handler does is limited to what
// POSIX calls async-signal-safe: the report is written with open/write into buffers made
// beforehand, nothing is allocated, and the signal is raised again at the end so that the
// program ends as it would have (and the system may still keep a core dump of it).

#if !defined(_WIN32)

#if !defined(__ANDROID__)
#include <execinfo.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>

#include "app/crash_handlers.hpp"
#include "app/crash_report.hpp"
#include "app/crash_text.hpp"

namespace redgem::crash {
namespace {

// (SIGBUS too: what the Mac sends for some of the bad memory accesses Linux sends SIGSEGV for.)
constexpr std::array<int, 5> kFatalSignals = {SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL};
// How a shell tells a program the signal ended: what a crash during the report ends with.
constexpr int kSignalExitBase = 128;
// The stack the handler runs on: its own, as a crash may be the program's stack running out.
constexpr std::size_t kHandlerStackBytes = 64 * 1024;
constexpr int kMaxFrames = 64;
constexpr std::size_t kMaxPathBytes = 4096;
constexpr mode_t kFolderMode = 0755;
constexpr mode_t kFileMode = 0644;
constexpr char kSeparator = '/';
constexpr const char* kLineEnd = "\n";

// Set while a crash is being reported: a second one (the handler crashing) ends the program at once.
volatile std::sig_atomic_t reporting = 0;

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

void write_backtrace(int fd) {
#if defined(__ANDROID__)
  write_text(fd, "  (none on this system)\n");
#else
  void* frames[kMaxFrames];
  const int count = backtrace(frames, kMaxFrames);
  for (int i = 0; i < count; ++i) {
    write_text(fd, kFrameIndent);
    backtrace_symbols_fd(&frames[i], 1, fd);
  }
#endif
}

void write_signal_report(int signal) {
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
  write_backtrace(fd);
  close(fd);
}

void on_fatal_signal(int signal) {
  if (reporting != 0) _exit(kSignalExitBase + signal);
  reporting = 1;
  write_signal_report(signal);
  write_text(STDERR_FILENO, prepared_crash().closed_message.c_str());
  write_text(STDERR_FILENO, kLineEnd);
  std::signal(signal, SIG_DFL);
  raise(signal);
}

void use_handler_stack() {
  static const std::unique_ptr<char[]> memory(new char[kHandlerStackBytes]);
  stack_t stack = {};
  stack.ss_sp = memory.get();
  stack.ss_size = kHandlerStackBytes;
  sigaltstack(&stack, nullptr);
}

}  // namespace

void install_fatal_handlers() {
  use_handler_stack();
  (void)current_backtrace();  // the first backtrace loads what it needs: not in a handler
  struct sigaction action = {};
  action.sa_handler = on_fatal_signal;
  action.sa_flags = SA_ONSTACK;
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
