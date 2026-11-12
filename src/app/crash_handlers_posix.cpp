// Crashes on Linux, Android and the Mac: the fatal signals. What a signal handler does is limited
// to what POSIX calls async-signal-safe: the report is made in a buffer set aside beforehand and
// written with open/write, nothing is allocated, and the signal is raised again at the end so that
// the program ends as it would have (and the system may still keep a core dump of it, or on
// Android its own tombstone).
//
// Each frame of the backtrace is written as an offset into its program ("Arena+0x1a2b"), which
// `redgem symbolize` turns into a function with the symbols of the build. (Where the frame is
// found is asked of the dynamic loader, dladdr, as the C library's own backtrace_symbols_fd does.)
// On Android the report also goes to the system's log (adb logcat).

#if !defined(_WIN32)

#if defined(__ANDROID__)
#include <android/log.h>
#include <sys/ucontext.h>
#include <unwind.h>
#else
#include <execinfo.h>
#endif
#include <dlfcn.h>
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
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "app/crash_handlers.hpp"
#include "app/crash_report.hpp"
#include "app/crash_text.hpp"
#include "core/build_id.hpp"
#include "core/platform.hpp"
#include "core/signal_stack.hpp"

namespace redgem::crash {
namespace {

// (SIGBUS too: what the Mac sends for some of the bad memory accesses Linux sends SIGSEGV for.)
constexpr std::array<int, 5> kFatalSignals = {SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL};
// How a shell tells a program the signal ended: what a crash during the report ends with.
constexpr int kSignalExitBase = 128;
constexpr int kMaxFrames = 64;
constexpr std::size_t kMaxPathBytes = 4096;
constexpr std::size_t kReportBytes = 64 * 1024;
constexpr mode_t kFolderMode = 0755;
constexpr mode_t kFileMode = 0644;
constexpr char kSeparator = '/';
constexpr char kLineBreak = '\n';
constexpr const char* kLineEnd = "\n";
constexpr const char* kNoFrames = "  (none: the system gave no frames)\n";
constexpr const char* kUnknownProgram = "?";
constexpr const char* kSymbolOpen = " (";
constexpr char kSymbolClose = ')';
#if defined(__ANDROID__)
constexpr const char* kLogTag = "redgem";  // as the engine's other messages (core/platform_log.cpp)
constexpr std::size_t kLogLineBytes = 1024;
#endif

// Set once a crash is being reported, and by which thread. (A lock-free atomic may be used in a
// signal handler.)
std::atomic<bool> reporting{false};
pthread_t reporter;
// How each fatal signal was handled before the engine's handler.
std::array<struct sigaction, kFatalSignals.size()> previous_handling{};
// The report of a crash, made where it lies: a handler may not allocate. (Set up before the
// program starts, so nothing in a handler sets it up.)
FixedText<kReportBytes> report;
// Something of this program's own, to ask the loader which program that is.
const char kProgramMark = 0;

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

bool names_no_file(const char* name) { return name == nullptr || *name == '\0'; }

// A path's last part.
const char* file_name_of(const char* path) {
  const char* slash = std::strrchr(path, kSeparator);
  return slash != nullptr ? slash + 1 : path;
}

// The name of a frame's program: the engine's as it was prepared, another's file name.
const char* program_name(const Dl_info& info) {
  if (info.dli_fbase == prepared_crash().program_base) return prepared_crash().program.c_str();
  return names_no_file(info.dli_fname) ? kUnknownProgram : file_name_of(info.dli_fname);
}

template <std::size_t Size>
void append_symbol(FixedText<Size>& text, const Dl_info& info) {
  if (info.dli_sname == nullptr) return;
  text.append(kSymbolOpen);
  text.append(info.dli_sname);
  text.append(kSymbolClose);
}

// A frame as a report says it: "  <program>+0x<offset> (<symbol>)", the symbol where the loader
// knows one.
template <std::size_t Size>
void append_frame(FixedText<Size>& text, const void* address) {
  Dl_info info{};
  const bool known = dladdr(address, &info) != 0;
  const auto at = reinterpret_cast<std::uintptr_t>(address);
  text.append(kFrameIndent);
  text.append(known ? program_name(info) : kUnknownProgram);
  text.append(kOffsetMark);
  text.append_hex(known ? at - reinterpret_cast<std::uintptr_t>(info.dli_fbase) : at);
  append_symbol(text, info);
  text.append(kLineBreak);
}

#if defined(__ANDROID__)
// (Android before 33 has no backtrace(): the unwinder of the C++ runtime walks the stack instead.)
struct Unwinding {
  void** frames = nullptr;
  int most = 0;
  int count = 0;
};

_Unwind_Reason_Code note_frame(_Unwind_Context* context, void* state) {
  auto& unwinding = *static_cast<Unwinding*>(state);
  if (unwinding.count >= unwinding.most) return _URC_END_OF_STACK;
  unwinding.frames[unwinding.count++] = reinterpret_cast<void*>(_Unwind_GetIP(context));
  return _URC_NO_REASON;
}

int this_threads_frames(void** frames, int most) {
  Unwinding unwinding;
  unwinding.frames = frames;
  unwinding.most = most;
  _Unwind_Backtrace(note_frame, &unwinding);
  return unwinding.count;
}

// Where the crashed thread was, as the signal's context says: the first frame, whatever the
// unwinder makes of the handler's own stack.
#if defined(__aarch64__)
void* crashed_address(const void* context) {
  return reinterpret_cast<void*>(static_cast<const ucontext_t*>(context)->uc_mcontext.pc);
}
#else
void* crashed_address(const void* context) {
  return reinterpret_cast<void*>(static_cast<const ucontext_t*>(context)->uc_mcontext.gregs[REG_RIP]);
}
#endif

int crashed_frames(void** frames, const void* context) {
  frames[0] = crashed_address(context);
  return 1 + this_threads_frames(frames + 1, kMaxFrames - 1);
}

// The report in the system's log as well, a line to an entry (an entry holds only so much).
void log_report() {
  FixedText<kLogLineBytes> line;
  for (const char* at = report.c_str(); *at != '\0'; ++at) {
    if (*at != kLineBreak) {
      line.append(*at);
      continue;
    }
    __android_log_write(ANDROID_LOG_FATAL, kLogTag, line.c_str());
    line.clear();
  }
}
#else
int this_threads_frames(void** frames, int most) { return backtrace(frames, most); }
void log_report() {}  // (the report is only a file: a terminal is told where it is)
#endif

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
int crashed_frames(void** frames, const void*) { return this_threads_frames(frames, kMaxFrames); }
#endif

void append_backtrace(const void* context) {
  void* frames[kMaxFrames];
  const int count = crashed_frames(frames, context);
  if (count <= 0) report.append(kNoFrames);
  for (int i = 0; i < count; ++i) append_frame(report, frames[i]);
}

void compose_report(int signal, const void* context, const Moment& moment) {
  report.append(prepared_crash().head.c_str());
  report.append(kTimeLabel);
  report.append(moment_text(moment).c_str());
  report.append(kCauseLabel);
  report.append(signal_description(signal));
  report.append(kBacktraceLabel);
  append_backtrace(context);
}

void write_signal_report(int signal, const void* context) {
  const Moment moment = utc_moment(static_cast<std::int64_t>(time(nullptr)));
  compose_report(signal, context, moment);
  log_report();
  ReportPath folder;
  folder.append(prepared_crash().folder_text.c_str());
  make_folders(folder);
  const int fd = open(report_path(moment).c_str(), O_WRONLY | O_CREAT | O_TRUNC, kFileMode);
  if (fd < 0) return;
  write_text(fd, report.c_str());
  close(fd);
}

// What the player is told: on the error stream, which a terminal shows (and on Android in the
// system's log, as no screen is left to show it on).
void tell_player(const char* message) {
  write_text(STDERR_FILENO, message);
  write_text(STDERR_FILENO, kLineEnd);
#if defined(__ANDROID__)
  __android_log_write(ANDROID_LOG_FATAL, kLogTag, message);
#endif
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

// Gives a signal back to what handled it before the engine (on Android the system's own crash
// reporter, which keeps a tombstone of the crash), or to the default, which ends the program.
void restore_previous_handling(int signal) {
  for (std::size_t i = 0; i < kFatalSignals.size(); ++i) {
    if (kFatalSignals[i] == signal) sigaction(signal, &previous_handling[i], nullptr);
  }
}

void on_fatal_signal(int signal, siginfo_t*, void* context) {
  if (reporting.exchange(true)) wait_for_the_report(signal);
  reporter = pthread_self();
  write_signal_report(signal, context);
  tell_player(prepared_crash().closed_message.c_str());
  restore_previous_handling(signal);
  raise(signal);
}

// A frame as current_backtrace gives it: without the report's indent and line end.
std::string frame_text(const void* address) {
  FixedText<kMaxPathBytes> line;
  append_frame(line, address);
  std::string text = line.c_str() + std::strlen(kFrameIndent);
  text.pop_back();
  return text;
}

}  // namespace

ProgramIdentity this_program() {
  Dl_info info{};
  ProgramIdentity program;
  program.name = platform::executable_path().filename().u8string();
  if (dladdr(&kProgramMark, &info) == 0) return program;
  program.base = info.dli_fbase;
  if (!names_no_file(info.dli_fname)) program.name = file_name_of(info.dli_fname);
  const auto identity = binary_identity_in_memory(info.dli_fbase);
  if (identity) program.build_id = identity->build_id;
  return program;
}

void install_fatal_handlers() {
  give_this_thread_a_signal_stack();
  (void)current_backtrace();  // the first backtrace loads what it needs: not in a handler
  struct sigaction action = {};
  action.sa_sigaction = on_fatal_signal;
  action.sa_flags = SA_ONSTACK | SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  for (std::size_t i = 0; i < kFatalSignals.size(); ++i) sigaction(kFatalSignals[i], &action, &previous_handling[i]);
}

std::vector<std::string> current_backtrace() {
  void* frames[kMaxFrames];
  const int count = this_threads_frames(frames, kMaxFrames);
  std::vector<std::string> lines;
  for (int i = 0; i < count; ++i) lines.push_back(frame_text(frames[i]));
  return lines;
}

void log_report_text(const std::string& text) {
  report.clear();
  report.append(text.c_str());
  log_report();
}

void show_player(const std::string& message) { tell_player(message.c_str()); }

}  // namespace redgem::crash

#endif
