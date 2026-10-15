#pragma once

#include <filesystem>
#include <string>

#include "core/log.hpp"

// The few things the engine asks the operating system for, in one place: everything else is
// the same code on Linux, Windows and the Mac.
namespace redgem::platform {

// The desktops the engine's tools run on, where each keeps the user's data and opens files its
// own way. (A phone counts as Linux: it is given its folder, see set_user_data_dir.)
enum class Desktop { Linux, Windows, Mac };
// The one this program runs on.
Desktop this_desktop();

// The file this program was started from.
std::filesystem::path executable_path();

// The user's home folder (a temporary folder when the system does not say).
std::filesystem::path home_dir();

// Where this user's RedGem data lives on this machine (settings, saves, the list of recent games):
// the folder given with set_user_data_dir, else where this desktop keeps it (user_data_dir_on).
std::filesystem::path user_data_dir();
// Where `desktop` keeps a user's RedGem data, for a user whose home folder is `home`: on the Mac,
// RedGem in Library/Application Support; elsewhere "redgem" in the desktop's settings folder,
// the one XDG_CONFIG_HOME names (`config_home`) or else .config.
std::filesystem::path user_data_dir_on(Desktop desktop, const std::filesystem::path& home,
                                       const std::string& config_home);
// Says where that folder is, for systems that hand each program a place of its own (a phone's
// apps). An empty path goes back to the usual one.
void set_user_data_dir(const std::filesystem::path& folder);

// What an environment variable of this process holds; nothing when it is not set, or set to nothing.
std::string environment(const std::string& name);
// Sets an environment variable of this process. With `overwrite` false an existing value stays.
void set_environment(const std::string& name, const std::string& value, bool overwrite);

// A text (a path, a name) as one word of a POSIX shell's command, whatever is in it.
std::string posix_shell_word(const std::filesystem::path& text);

// The shell command that opens a file or folder with the desktop's default application, without
// waiting for it: this desktop's (open_command_on).
std::string open_command(const std::filesystem::path& path);
// That command on `desktop`: xdg-open on Linux, open on the Mac, start on Windows.
std::string open_command_on(Desktop desktop, const std::filesystem::path& path);

// Shows one of the engine's messages where this system shows a program's: the terminal (errors
// and warnings on its error stream), or the system's own log where programs have no terminal (a
// phone: adb logcat). What log:: writes to until someone installs a sink of their own.
void write_log(log::Level level, const std::string& message);
// A message as write_log shows it. What a message says may come from a game's scripts and files,
// which are not trusted: its control characters (a terminal's escapes) become '?', and the lines
// after its first are set in from the margin, so that nothing inside a message drives the
// terminal or reads as a message of the engine's own.
std::string log_text(const std::string& message);

// Whether this program runs on Windows: what it builds games for by default, and how files are named.
bool is_windows();
// Whether it runs on a Mac: then it builds games for the Mac by default.
bool is_mac();
// Whether other programs can be run through a POSIX shell here (the Android SDK's tools are).
bool has_posix_shell();

// Opens a file or folder with the desktop's default application, without waiting. A warning says
// so when it could not.
void open_in_system(const std::filesystem::path& path);

// File name of an executable called `name` ("game" -> "game.exe" on Windows).
std::string executable_file_name(const std::string& name, bool for_windows);

}  // namespace redgem::platform
