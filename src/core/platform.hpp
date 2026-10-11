#pragma once

#include <filesystem>
#include <string>

// The few things the engine asks the operating system for, in one place: everything else is
// the same code on Linux and Windows.
namespace redgem::platform {

// The file this program was started from.
std::filesystem::path executable_path();

// The user's home folder (a temporary folder when the system does not say).
std::filesystem::path home_dir();

// Sets an environment variable of this process. With `overwrite` false an existing value stays.
void set_environment(const std::string& name, const std::string& value, bool overwrite);

// The shell command that opens a file or folder with the desktop's default application, without
// waiting for it.
std::string open_command(const std::filesystem::path& path);

// File name of an executable called `name` ("game" -> "game.exe" on Windows).
std::string executable_file_name(const std::string& name, bool for_windows);

}  // namespace redgem::platform
