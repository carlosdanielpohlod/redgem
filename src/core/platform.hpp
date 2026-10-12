#pragma once

#include <filesystem>
#include <string>

#include "core/log.hpp"

// The few things the engine asks the operating system for, in one place: everything else is
// the same code on Linux and Windows.
namespace redgem::platform {

// The file this program was started from.
std::filesystem::path executable_path();

// The user's home folder (a temporary folder when the system does not say).
std::filesystem::path home_dir();

// Where this user's RedGem data lives on this machine (settings, saves, the list of recent games):
// the folder given with set_user_data_dir, else "redgem" in the desktop's settings folder
// (XDG_CONFIG_HOME, or .config in the home folder).
std::filesystem::path user_data_dir();
// Says where that folder is, for systems that hand each program a place of its own (a phone's
// apps). An empty path goes back to the usual one.
void set_user_data_dir(const std::filesystem::path& folder);

// Sets an environment variable of this process. With `overwrite` false an existing value stays.
void set_environment(const std::string& name, const std::string& value, bool overwrite);

// The shell command that opens a file or folder with the desktop's default application, without
// waiting for it.
std::string open_command(const std::filesystem::path& path);

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
// Whether other programs can be run through a POSIX shell here (the Android SDK's tools are).
bool has_posix_shell();

// Opens a file or folder with the desktop's default application, without waiting. A warning says
// so when it could not.
void open_in_system(const std::filesystem::path& path);

// File name of an executable called `name` ("game" -> "game.exe" on Windows).
std::string executable_file_name(const std::string& name, bool for_windows);

}  // namespace redgem::platform
