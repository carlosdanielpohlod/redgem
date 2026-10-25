#include "app/crash_report.hpp"

#include <cstdlib>
#include <ctime>
#include <exception>
#include <fstream>
#include <memory>
#include <new>

#include "app/crash_handlers.hpp"
#include "app/crash_text.hpp"
#include "app/game_settings.hpp"
#include "app/save_location.hpp"
#include "core/engine_thread.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/translations.hpp"

namespace redgem::crash {
namespace {

constexpr const char* kTitleLine = "RedGem crash report\n";
constexpr const char* kFolderSuffix = "-crashes";
// What the report says when the stack was gone by the time the crash was seen (an exception that
// reached the program's top had already left every function it came through).
constexpr const char* kNoBacktrace = "  (none: the stack had already unwound)\n";
constexpr const char* kExceptionCause = "uncaught exception: ";
constexpr const char* kOutOfMemoryCause = "out of memory (std::bad_alloc)";
// Memory kept from the start and given back first when memory runs out: what the report and the
// player's message are then made with.
constexpr std::size_t kMemoryReserveBytes = 1024 * 1024;
constexpr std::size_t kPageBytes = 4096;  // the smallest page of the systems the engine runs on
constexpr char kTouched = 1;

#if defined(_WIN32)
constexpr const char* kSystem = "Windows";
#elif defined(__APPLE__)
constexpr const char* kSystem = "macOS";
#elif defined(__ANDROID__)
constexpr const char* kSystem = "Android";
#else
constexpr const char* kSystem = "Linux";
#endif
#if defined(__x86_64__) || defined(_M_X64)
constexpr const char* kProcessor = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
constexpr const char* kProcessor = "arm64";
#else
constexpr const char* kProcessor = "other processor";
#endif

#ifdef REDGEM_VERSION
constexpr const char* kVersion = REDGEM_VERSION;
#else
constexpr const char* kVersion = "unknown";
#endif

PreparedCrash& prepared() {
  static PreparedCrash crash;
  return crash;
}

std::unique_ptr<char[]>& memory_reserve() {
  static std::unique_ptr<char[]> reserve;
  return reserve;
}

void release_memory_reserve() { memory_reserve().reset(); }

// Writes a byte of every page, so that the memory is the system's to give back when it is freed
// (memory never written may never have been given: Linux and Android hand it out on first use).
void touch_pages(char* memory, std::size_t bytes) {
  volatile char* const pages = memory;  // (written through volatile: not a store to leave out)
  for (std::size_t at = 0; at < bytes; at += kPageBytes) pages[at] = kTouched;
}

bool& installed() {
  static bool done = false;
  return done;
}

std::string now_text() { return moment_text(utc_moment(static_cast<std::int64_t>(std::time(nullptr)))).c_str(); }

std::string backtrace_lines(const std::vector<std::string>& backtrace) {
  if (backtrace.empty()) return kNoBacktrace;
  std::string lines;
  for (const std::string& frame : backtrace) lines += kFrameIndent + frame + "\n";
  return lines;
}

// The lines of a report that say what happened, and when.
std::string report_moment(const std::string& time, const std::string& cause,
                          const std::vector<std::string>& backtrace) {
  return kTimeLabel + time + kCauseLabel + cause + kBacktraceLabel + backtrace_lines(backtrace);
}

// What a player is told, in their language: the game closed, and where the report is.
std::string player_message(const char* key, const std::string& game, const std::string& folder) {
  return tr(key, {{"game", game}, {"folder", folder}});
}

void prepare(const CrashSetup& setup) {
  CrashFacts facts;
  facts.game = read_game_settings(setup.game_dir).title;
  facts.version = kVersion;
  facts.platform = platform_name();
  PreparedCrash& crash = prepared();
  crash.folder = crash_folder_for(save_file_for(setup.game_dir));
  // (In UTF-8: on Windows a folder's name may hold letters the system's code page has none for.)
  crash.folder_text = crash.folder.u8string();
  crash.head = report_head(facts);
  crash.closed_message = player_message("game.crash.closed", facts.game, crash.folder_text);
  crash.out_of_memory_message = player_message("game.crash.out_of_memory", facts.game, crash.folder_text);
  crash.show_dialog = setup.show_dialog;
}

// Whether an exception nobody caught is memory running out.
bool out_of_memory(const std::exception_ptr& uncaught) {
  try {
    if (uncaught) std::rethrow_exception(uncaught);
  } catch (const std::bad_alloc&) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

// What an exception nobody caught says of itself.
std::string exception_text(const std::exception_ptr& uncaught) {
  try {
    if (uncaught) std::rethrow_exception(uncaught);
    return "none (std::terminate was called directly)";
  } catch (const std::exception& error) {
    return error.what();
  } catch (...) {
    return "not a std::exception";
  }
}

// Reports an exception nobody caught (memory running out as such) and ends the program.
[[noreturn]] void report_uncaught(const std::exception_ptr& uncaught) {
  if (out_of_memory(uncaught)) {
    release_memory_reserve();
    report_and_end(kOutOfMemoryCause, current_backtrace(), prepared().out_of_memory_message);
  }
  report_and_end(kExceptionCause + exception_text(uncaught), current_backtrace(), prepared().closed_message);
}

[[noreturn]] void on_terminate() { report_uncaught(std::current_exception()); }

// What leaves the work of a thread the engine started (core/engine_thread.hpp).
[[noreturn]] void on_thread_exception(const std::exception_ptr& uncaught) { report_uncaught(uncaught); }

// Writes the report and tells the player; the exit code a crash ends with.
int report_and_tell(const std::string& cause, const std::string& message) {
  log::error(cause);
  if (!installed()) return kExitFailure;
  write_report(cause, {});
  show_player(message);
  return kExitFailure;
}

}  // namespace

const PreparedCrash& prepared_crash() { return prepared(); }

std::string report_head(const CrashFacts& facts) {
  return std::string(kTitleLine) + "Game: " + facts.game + "\nEngine: " + facts.version +
         "\nPlatform: " + facts.platform + "\n";
}

std::string format_crash_report(const CrashFacts& facts) {
  return report_head(facts) + report_moment(facts.time, facts.cause, facts.backtrace);
}

std::filesystem::path crash_folder_for(const std::filesystem::path& save_file) {
  std::filesystem::path name = save_file.stem();
  name += kFolderSuffix;
  return save_file.parent_path() / name;
}

std::string crash_file_name(long long seconds_since_epoch) {
  return std::string(kReportPrefix) + moment_stamp(utc_moment(seconds_since_epoch)).c_str() + kReportExtension;
}

const char* engine_version() { return kVersion; }

const char* platform_name() {
  static const std::string name = std::string(kSystem) + " " + kProcessor;
  return name.c_str();
}

void install_crash_handling(const CrashSetup& setup) {
  memory_reserve().reset(new char[kMemoryReserveBytes]);
  touch_pages(memory_reserve().get(), kMemoryReserveBytes);
  prepare(setup);
  std::set_terminate(on_terminate);
  set_uncaught_handler(on_thread_exception);
  install_fatal_handlers();
  installed() = true;
}

bool crash_handling_installed() { return installed(); }

std::filesystem::path write_report(const std::string& cause, const std::vector<std::string>& backtrace) {
  const std::filesystem::path& folder = prepared().folder;
  std::error_code fs_error;
  std::filesystem::create_directories(folder, fs_error);
  const std::filesystem::path file = folder / crash_file_name(static_cast<long long>(std::time(nullptr)));
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  out << prepared().head << report_moment(now_text(), cause, backtrace);
  return out.good() ? file : std::filesystem::path();
}

void report_and_end(const std::string& cause, const std::vector<std::string>& backtrace, const std::string& message) {
  write_report(cause, backtrace);
  show_player(message);
  std::_Exit(kExitFailure);
}

int end_with_exception(const std::string& what) {
  return report_and_tell(kExceptionCause + what, prepared().closed_message);
}

int end_out_of_memory() {
  release_memory_reserve();
  try {
    return report_and_tell(kOutOfMemoryCause, prepared().out_of_memory_message);
  } catch (const std::bad_alloc&) {
    return kExitFailure;  // (not even enough for the report: the player is left the exit code)
  }
}

}  // namespace redgem::crash
