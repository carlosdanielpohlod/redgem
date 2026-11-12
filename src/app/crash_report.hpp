#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// What a shipped game does when it crashes: it writes a report of what happened in the crashes
// folder of its players' folder (see app/game_data_location.hpp), tells the player in a few words
// where it is, and ends with a failure.
//
// A crash is a fatal signal (SIGSEGV, SIGABRT, SIGFPE, SIGILL; on Windows the system's own
// exceptions), an exception nobody caught (std::terminate) or one that reached the program's
// top. The save needs nothing done for it: it is written whole to the disk on every change
// (storage/save_file.hpp), so a crash loses nothing of it.
//
// A report names the build that crashed by the id its linker gave it (core/build_id.hpp), and the
// frames of its backtrace as offsets into the program ("Arena.exe+0x1a2b"): `redgem symbolize`
// turns those into functions with the symbols kept of that build (project/crash_symbols.hpp).
namespace redgem::crash {

// The line of a report that names the build: "Build: <id> <program>", the program as the frames
// of the backtrace name it. The id is kUnknownBuild where the program carries none (the Mac).
inline constexpr const char* kBuildLabel = "Build: ";
inline constexpr const char* kUnknownBuild = "unknown";
// How a frame of a backtrace says where it is: <program>+0x<offset>, and then, where the system
// can tell, " (<symbol>)".
inline constexpr const char* kOffsetMark = "+0x";
inline constexpr const char* kSymbolMark = " (";

// What a report says.
struct CrashFacts {
  std::string game;                    // the game's title
  std::string version;                 // the engine's
  std::string platform;                // the system, and its kind of processor
  std::string build_id;                // the program's (core/build_id.hpp); empty when it has none
  std::string program;                 // the program (or library) the engine is, as frames name it
  std::string time;                    // when it happened, in UTC
  std::string cause;                   // the signal or the exception
  std::vector<std::string> backtrace;  // where the program was, innermost first; may be empty
};

// The lines of a report that are known before anything goes wrong (game, engine, platform).
std::string report_head(const CrashFacts& facts);
// The report, whole.
std::string format_crash_report(const CrashFacts& facts);

// The name of the report of a crash at a moment ("crash-20261024-130509.log").
std::string crash_file_name(long long seconds_since_epoch);

// The newest report in a crashes folder that nobody was told of yet, which it notes as told (in
// kToldFile, beside the reports): how a game's next run knows the one before it crashed, on
// Android too, where the crash itself could tell its player nothing. Nothing when there is none.
inline constexpr const char* kToldFile = "told.txt";
std::optional<std::filesystem::path> untold_crash_report(const std::filesystem::path& folder);

// The engine's version and what it was built for, as a report says them.
const char* engine_version();
const char* platform_name();

struct CrashSetup {
  std::filesystem::path game_dir;
  // The game's players' folder, where its save is (see app/game_data_location.hpp): empty for the
  // one players_data_folder says.
  std::filesystem::path data_folder;
  // Whether the player is shown a message box where the system has them (Windows): off for tests.
  bool show_dialog = true;
};
// The folder a game's crash reports go in: crashes/ in its players' folder, beside its save.
std::filesystem::path crash_folder_for(const CrashSetup& setup);

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
