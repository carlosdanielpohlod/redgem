// Crashes on Windows: what the system raises (access violations, a stack that ran out, illegal
// instructions, division by zero) reaches the unhandled exception filter, and abort() the
// SIGABRT handler. Neither is a POSIX signal handler: the report is written with the ordinary
// functions, as well as a crashed program still can, and the player is shown a message box (a
// game has no console to read an error in).

#if defined(_WIN32)

#include <windows.h>

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "app/crash_handlers.hpp"
#include "app/crash_report.hpp"
#include "app/crash_text.hpp"

namespace redgem::crash {
namespace {

constexpr int kMaxFrames = 62;  // the most CaptureStackBackTrace takes on older systems
// What is kept of the stack for the filter to run on when the crash is the stack running out.
constexpr ULONG kHandlerStackBytes = 64 * 1024;
constexpr int kAddressDigits = 16;
constexpr const char* kDialogTitle = "RedGem";
constexpr const char* kPathSeparators = "\\/";

struct ExceptionName {
  DWORD code;
  const char* text;
};
constexpr ExceptionName kExceptionNames[] = {
    {EXCEPTION_ACCESS_VIOLATION, "EXCEPTION_ACCESS_VIOLATION (invalid memory access)"},
    {EXCEPTION_STACK_OVERFLOW, "EXCEPTION_STACK_OVERFLOW (the stack ran out)"},
    {EXCEPTION_ILLEGAL_INSTRUCTION, "EXCEPTION_ILLEGAL_INSTRUCTION (illegal instruction)"},
    {EXCEPTION_INT_DIVIDE_BY_ZERO, "EXCEPTION_INT_DIVIDE_BY_ZERO (division by zero)"},
    {EXCEPTION_IN_PAGE_ERROR, "EXCEPTION_IN_PAGE_ERROR (memory that could not be read in)"},
};

std::string exception_description(DWORD code) {
  for (const ExceptionName& name : kExceptionNames) {
    if (name.code == code) return name.text;
  }
  char hex[kAddressDigits + 1];
  std::snprintf(hex, sizeof(hex), "%08lX", static_cast<unsigned long>(code));
  return std::string("exception 0x") + hex;
}

// Where an address is: the module it belongs to and how far into it ("redgem-player.exe+0x1a2b").
std::string frame_text(void* address) {
  HMODULE module = nullptr;
  char name[MAX_PATH] = "?";
  const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
  if (GetModuleHandleExA(flags, static_cast<LPCSTR>(address), &module)) GetModuleFileNameA(module, name, MAX_PATH);
  const auto offset = reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(module);
  char hex[kAddressDigits + 1];
  std::snprintf(hex, sizeof(hex), "%llx", static_cast<unsigned long long>(offset));
  const std::string path = name;
  return path.substr(path.find_last_of(kPathSeparators) + 1) + "+0x" + hex;
}

LONG WINAPI on_unhandled_exception(EXCEPTION_POINTERS* info) {
  report_and_end(exception_description(info->ExceptionRecord->ExceptionCode), current_backtrace());
}

void on_abort(int signal) { report_and_end(signal_description(signal), current_backtrace()); }

std::wstring wide(const std::string& text) {
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<std::size_t>(length > 0 ? length : 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), length);
  return out;
}

}  // namespace

void install_fatal_handlers() {
  ULONG reserved = kHandlerStackBytes;
  SetThreadStackGuarantee(&reserved);
  SetUnhandledExceptionFilter(on_unhandled_exception);
  // (No "abnormal program termination" box of the C library's own: the game shows its own.)
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  std::signal(SIGABRT, on_abort);
}

std::vector<std::string> current_backtrace() {
  void* frames[kMaxFrames];
  const USHORT count = CaptureStackBackTrace(0, kMaxFrames, frames, nullptr);
  std::vector<std::string> lines;
  for (USHORT i = 0; i < count; ++i) lines.push_back(frame_text(frames[i]));
  return lines;
}

void show_player(const std::string& message) {
  std::fprintf(stderr, "%s\n", message.c_str());
  if (!prepared_crash().show_dialog) return;
  MessageBoxW(nullptr, wide(message).c_str(), wide(kDialogTitle).c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
}

}  // namespace redgem::crash

#endif
