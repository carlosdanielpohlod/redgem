#pragma once

#include <filesystem>
#include <string>
#include <vector>

// What a shipped game does when it crashes: it writes a report of what happened beside the
// game's save, tells the player in a few words where it is, and ends with a failure.
//
// A crash is a fatal signal (SIGSEGV, SIGABRT, SIGFPE, SIGILL; on Windows the system's own
// exceptions), an exception nobody caught (std::terminate) or one that reached the program's
// top. The save needs nothing done for it: it is written whole to the disk on every change
// (storage/save_file.hpp), so a crash loses nothing of it.
namespace redgem::crash {

// What a report says.
struct CrashFacts {
  std::string game;                    // the game's title
  std::string version;                 // the engine's
  std::string platform;                // the system, and its kind of processor
  std::string time;                    // when it happened, in UTC
  std::string cause;                   // the signal or the exception
  std::vector<std::string> backtrace;  // where the program was, innermost first; may be empty
};

// The lines of a report that are known before anything goes wrong (game, engine, platform).
std::string report_head(const CrashFacts& facts);
// The report, whole.
std::string format_crash_report(const CrashFacts& facts);

// The folder a game's crash reports go in: beside its save, named after it
// (saves/by-id/<id>-crashes/ for the save saves/by-id/<id>.json).
std::filesystem::path crash_folder_for(const std::filesystem::path& save_file);
// The name of the report of a crash at a moment ("crash-20261024-130509.log").
std::string crash_file_name(long long seconds_since_epoch);

// The engine's version and what it was built for, as a report says them.
const char* engine_version();
const char* platform_name();

struct CrashSetup {
  std::filesystem::path game_dir;
  // Whether the player is shown a message box where the system has them (Windows): off for tests.
  bool show_dialog = true;
};
// Prepares everything a crash will need (where the report goes, what the player is told) and
// installs the handlers. Once, before the game starts.
void install_crash_handling(const CrashSetup& setup);
bool crash_handling_installed();

// For the program's top: an exception came out of the game. Reports it and tells the player
// (when crash handling is installed; otherwise it is only logged). Returns the exit code.
int end_with_exception(const std::string& what);
// The same for running out of memory, with a message of its own.
int end_out_of_memory();

}  // namespace redgem::crash
