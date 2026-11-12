#pragma once

#include <filesystem>
#include <string>
#include <vector>

// What crash_report.cpp shares with the handlers of each system (crash_handlers_posix.cpp,
// crash_handlers_windows.cpp).
namespace redgem::crash {

// How a report names what it says after its head (see format_crash_report).
inline constexpr const char* kTimeLabel = "Time: ";
inline constexpr const char* kCauseLabel = "\nCause: ";
inline constexpr const char* kBacktraceLabel = "\nBacktrace:\n";
inline constexpr const char* kFrameIndent = "  ";
// A report's file is <prefix><moment_stamp><extension>.
inline constexpr const char* kReportPrefix = "crash-";
inline constexpr const char* kReportExtension = ".log";

// What install_crash_handling prepared, for a crash to use as it is: a signal handler may not
// allocate, so nothing it writes is made at that point.
struct PreparedCrash {
  std::filesystem::path folder;        // where reports go
  std::string folder_text;             // the same in UTF-8: for a signal handler, and the player
  std::string head;                    // report_head of the game
  std::string closed_message;          // what the player is told after a crash
  std::string out_of_memory_message;   // ...after running out of memory
  std::string program;                 // the engine's program, as the frames of a backtrace name it
  const void* program_base = nullptr;  // where the system loaded it
  bool show_dialog = true;
};
const PreparedCrash& prepared_crash();

// The program the engine is (on Android, the library the app loads): its name as a backtrace says
// it, the id of its build, and where the system loaded it.
struct ProgramIdentity {
  std::string name;
  std::string build_id;  // empty when it carries none
  const void* base = nullptr;
};

// Each system's own.
ProgramIdentity this_program();
void install_fatal_handlers();
// Where the program is now, innermost first (empty where the system cannot say).
std::vector<std::string> current_backtrace();
// A message box where the system has one for a program with no window left (Windows); the error
// stream elsewhere (and on Android the system's log).
void show_player(const std::string& message);
// A report in the system's log as well, where the system keeps one apps write to (Android's adb
// logcat); nothing elsewhere.
void log_report_text(const std::string& text);

// Writes the report of a crash happening now; where it went, or nothing when it could not be
// written. Not for a signal handler (it allocates).
std::filesystem::path write_report(const std::string& cause, const std::vector<std::string>& backtrace);
// Writes the report, tells the player `message` and ends the program (for std::terminate).
[[noreturn]] void report_and_end(const std::string& cause, const std::vector<std::string>& backtrace,
                                 const std::string& message);

}  // namespace redgem::crash
