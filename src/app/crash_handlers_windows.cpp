// Crashes on Windows: what the system raises (access violations, a stack that ran out, illegal
// instructions, division by zero) reaches the unhandled exception filter, and abort() the
// SIGABRT handler. A crashed program may have no stack left, or a damaged heap: the filter only
// notes where the program was and hands the report to a thread made for it when the game
// started, which writes it from buffers made then (CreateFile and WriteFile, no allocation) and
// shows the player a message box looked up then too.

#if defined(_WIN32)

#include <windows.h>

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "app/crash_handlers.hpp"
#include "app/crash_report.hpp"
#include "app/crash_text.hpp"
#include "core/exit_codes.hpp"
#include "core/signal_stack.hpp"

namespace redgem::crash {
namespace {

constexpr int kMaxFrames = 62;  // the most CaptureStackBackTrace takes on older systems
constexpr std::size_t kReportBytes = 64 * 1024;
constexpr std::size_t kMaxPathCharacters = 32768;  // Windows' longest path
constexpr const wchar_t* kDialogTitle = L"RedGem";
// Where the message box comes from: looked up as the game starts, as user32 linked with the
// engine would clash with raylib's functions of the same names (CloseWindow, ShowCursor...).
constexpr const wchar_t* kUserLibrary = L"user32.dll";
constexpr const char* kMessageBoxFunction = "MessageBoxW";
constexpr UINT kDialogStyle = MB_OK | MB_ICONERROR | MB_TOPMOST;
using MessageBoxFunction = int(WINAPI*)(HWND, LPCWSTR, LPCWSTR, UINT);
constexpr wchar_t kSeparator = L'\\';
constexpr const char* kPathSeparators = "\\/";
// FILETIME counts 100 ns steps from 1601; a report counts seconds from 1970.
constexpr std::uint64_t kFileTimeStepsPerSecond = 10 * 1000 * 1000;
constexpr std::uint64_t kSecondsFrom1601To1970 = 11644473600ULL;

struct ExceptionName {
  DWORD code = 0;
  const char* text = nullptr;
};
// What a C++ exception is to Windows while it looks for a catch (this toolchain's, as GCC's: "GCC"
// in its letters). One nobody catches can reach the filter as that, before std::terminate.
constexpr DWORD kCxxExceptionCode = 0x20474343;
constexpr ExceptionName kExceptionNames[] = {
    {kCxxExceptionCode, "uncaught exception (a C++ exception nobody caught)"},
    {EXCEPTION_ACCESS_VIOLATION, "EXCEPTION_ACCESS_VIOLATION (invalid memory access)"},
    {EXCEPTION_STACK_OVERFLOW, "EXCEPTION_STACK_OVERFLOW (the stack ran out)"},
    {EXCEPTION_ILLEGAL_INSTRUCTION, "EXCEPTION_ILLEGAL_INSTRUCTION (illegal instruction)"},
    {EXCEPTION_INT_DIVIDE_BY_ZERO, "EXCEPTION_INT_DIVIDE_BY_ZERO (division by zero)"},
    {EXCEPTION_IN_PAGE_ERROR, "EXCEPTION_IN_PAGE_ERROR (memory that could not be read in)"},
};

// What the reporter needs, all made as the game starts.
struct Reporter {
  std::wstring folder;   // where reports go
  std::wstring message;  // what the player is told
  MessageBoxFunction message_box = nullptr;
  HANDLE asked = nullptr;     // set by the filter: a crash to report
  HANDLE reported = nullptr;  // set by the reporter once it is written and told
  LONG taken = 0;             // set by the first crash: a second one waits for its report
  // The crash, as the filter saw it.
  const char* cause = nullptr;
  DWORD code = 0;
  void* frames[kMaxFrames] = {};
  USHORT frame_count = 0;
  FixedText<kReportBytes> text;
};

Reporter& reporter() {
  static Reporter* made = new Reporter();  // (never freed: it is used as the program ends)
  return *made;
}

std::wstring wide(const std::string& text) {
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<std::size_t>(length > 0 ? length : 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), length);
  return out;
}

const char* exception_name(DWORD code) {
  for (const ExceptionName& name : kExceptionNames) {
    if (name.code == code) return name.text;
  }
  return nullptr;
}

std::int64_t seconds_now() {
  FILETIME now{};
  GetSystemTimeAsFileTime(&now);
  const std::uint64_t steps = (static_cast<std::uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
  return static_cast<std::int64_t>(steps / kFileTimeStepsPerSecond - kSecondsFrom1601To1970);
}

// The cause, by name or by its code.
void append_cause(Reporter& crash) {
  const char* name = crash.cause != nullptr ? crash.cause : exception_name(crash.code);
  if (name != nullptr) {
    crash.text.append(name);
    return;
  }
  crash.text.append("exception 0x");
  crash.text.append_hex(crash.code);
}

constexpr std::size_t kFrameLineBytes = MAX_PATH * 4;

// Where an address is: the module it belongs to and how far into it ("redgem-player.exe+0x1a2b").
template <std::size_t Size>
void append_frame(FixedText<Size>& text, void* address) {
  HMODULE module = nullptr;
  wchar_t wide_name[MAX_PATH] = L"?";
  const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
  if (GetModuleHandleExW(flags, static_cast<LPCWSTR>(address), &module))
    GetModuleFileNameW(module, wide_name, MAX_PATH);
  char name[MAX_PATH * 3] = "?";
  WideCharToMultiByte(CP_UTF8, 0, wide_name, -1, name, sizeof(name), nullptr, nullptr);
  const char* base = name;
  for (const char* at = name; *at != '\0'; ++at) {
    if (std::strchr(kPathSeparators, *at) != nullptr) base = at + 1;
  }
  text.append(kFrameIndent);
  text.append(base);
  text.append("+0x");
  text.append_hex(reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(module));
  text.append('\n');
}

void compose(Reporter& crash, const Moment& moment) {
  crash.text.append(prepared_crash().head.c_str());
  crash.text.append(kTimeLabel);
  crash.text.append(moment_text(moment).c_str());
  crash.text.append(kCauseLabel);
  append_cause(crash);
  crash.text.append(kBacktraceLabel);
  for (USHORT i = 0; i < crash.frame_count; ++i) append_frame(crash.text, crash.frames[i]);
}

// Makes every folder of the path that is not there yet (in a buffer of the stack: no heap).
void make_folders(const std::wstring& folder) {
  wchar_t path[kMaxPathCharacters];
  const std::size_t length = folder.copy(path, kMaxPathCharacters - 1);
  path[length] = L'\0';
  for (std::size_t i = 1; i < length; ++i) {
    if (path[i] != kSeparator && path[i] != L'/') continue;
    path[i] = L'\0';
    CreateDirectoryW(path, nullptr);
    path[i] = kSeparator;
  }
  CreateDirectoryW(path, nullptr);
}

void write_file(Reporter& crash, const Moment& moment) {
  make_folders(crash.folder);
  wchar_t path[kMaxPathCharacters];
  const std::size_t length = crash.folder.copy(path, kMaxPathCharacters - 1);
  std::size_t at = length;
  FixedText<kMomentTextSize * 2> name;
  name.append(kReportPrefix);
  name.append(moment_stamp(moment).c_str());
  name.append(kReportExtension);
  if (at + name.size() + 2 > kMaxPathCharacters) return;
  path[at++] = kSeparator;
  for (const char* letter = name.c_str(); *letter != '\0'; ++letter) path[at++] = *letter;  // (all ASCII)
  path[at] = L'\0';
  const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;
  DWORD written = 0;
  WriteFile(file, crash.text.c_str(), static_cast<DWORD>(crash.text.size()), &written, nullptr);
  CloseHandle(file);
}

void tell(const Reporter& crash) {
  WriteFile(GetStdHandle(STD_ERROR_HANDLE), prepared_crash().closed_message.c_str(),
            static_cast<DWORD>(prepared_crash().closed_message.size()), nullptr, nullptr);
  if (!prepared_crash().show_dialog || crash.message_box == nullptr) return;
  crash.message_box(nullptr, crash.message.c_str(), kDialogTitle, kDialogStyle);
}

// The reporter's thread: asleep until a crash, on a stack of its own.
DWORD WINAPI report_crashes(void*) {
  Reporter& crash = reporter();
  WaitForSingleObject(crash.asked, INFINITE);
  const Moment moment = utc_moment(seconds_now());
  compose(crash, moment);
  write_file(crash, moment);
  tell(crash);
  SetEvent(crash.reported);
  return 0;
}

// Notes where the crashed thread is, has the reporter write the report, and waits for it.
void hand_over(const char* cause, DWORD code) {
  Reporter& crash = reporter();
  if (InterlockedExchange(&crash.taken, 1) != 0) {
    WaitForSingleObject(crash.reported, INFINITE);
    return;
  }
  crash.cause = cause;
  crash.code = code;
  crash.frame_count = CaptureStackBackTrace(0, kMaxFrames, crash.frames, nullptr);
  SetEvent(crash.asked);
  WaitForSingleObject(crash.reported, INFINITE);
}

LONG WINAPI on_unhandled_exception(EXCEPTION_POINTERS* info) {
  hand_over(nullptr, info->ExceptionRecord->ExceptionCode);
  return EXCEPTION_EXECUTE_HANDLER;  // (the program ends, with the exception's code)
}

void on_abort(int signal) {
  hand_over(signal_description(signal), 0);
  std::_Exit(kExitFailure);
}

void prepare_reporter() {
  Reporter& crash = reporter();
  crash.folder = prepared_crash().folder.wstring();
  crash.message = wide(prepared_crash().closed_message);
  const HMODULE user = LoadLibraryW(kUserLibrary);
  if (user != nullptr) {
    crash.message_box = reinterpret_cast<MessageBoxFunction>(GetProcAddress(user, kMessageBoxFunction));
  }
  crash.asked = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  crash.reported = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  CreateThread(nullptr, 0, report_crashes, nullptr, 0, nullptr);
}

}  // namespace

void install_fatal_handlers() {
  give_this_thread_a_signal_stack();
  prepare_reporter();
  SetUnhandledExceptionFilter(on_unhandled_exception);
  // (No "abnormal program termination" box of the C library's own: the game shows its own.)
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  std::signal(SIGABRT, on_abort);
}

std::vector<std::string> current_backtrace() {
  void* frames[kMaxFrames];
  const USHORT count = CaptureStackBackTrace(0, kMaxFrames, frames, nullptr);
  std::vector<std::string> lines;
  for (USHORT i = 0; i < count; ++i) {
    FixedText<kFrameLineBytes> line;
    append_frame(line, frames[i]);
    std::string text = line.c_str() + std::strlen(kFrameIndent);
    text.pop_back();  // (its line end: the report adds its own)
    lines.push_back(text);
  }
  return lines;
}

void show_player(const std::string& message) {
  std::fprintf(stderr, "%s\n", message.c_str());
  const Reporter& crash = reporter();
  if (!prepared_crash().show_dialog || crash.message_box == nullptr) return;
  crash.message_box(nullptr, wide(message).c_str(), kDialogTitle, kDialogStyle);
}

}  // namespace redgem::crash

#endif
