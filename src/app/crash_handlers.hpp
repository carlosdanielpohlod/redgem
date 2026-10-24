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
  std::filesystem::path folder;       // where reports go
  std::string folder_text;            // the same in UTF-8: for a signal handler, and the player
  std::string head;                   // report_head of the game
  std::string closed_message;         // what the player is told after a crash
  std::string out_of_memory_message;  // ...after running out of memory
  bool show_dialog = true;
};
const PreparedCrash& prepared_crash();

// Each system's own.
void install_fatal_handlers();
// Where the program is now, innermost first (empty where the system cannot say).
std::vector<std::string> current_backtrace();
// A message box where the system has one for a program with no window left (Windows); the error
// stream elsewhere.
void show_player(const std::string& message);

// Writes the report of a crash happening now; where it went, or nothing when it could not be
// written. Not for a signal handler (it allocates).
std::filesystem::path write_report(const std::string& cause, const std::vector<std::string>& backtrace);
// Writes the report, tells the player `message` and ends the program (for std::terminate).
[[noreturn]] void report_and_end(const std::string& cause, const std::vector<std::string>& backtrace,
                                 const std::string& message);

}  // namespace redgem::crash
