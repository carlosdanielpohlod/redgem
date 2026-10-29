#pragma once

// A program started with its standard streams connected to the editor's: how a coding agent runs
// beside the editor. What it writes is read on threads of the caller's (each read waits), what it is
// sent is written in whole lines; stopping it ends whatever it started too (npx starts node, which
// starts the agent).

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace redgem::agent {

// One variable of the started program's environment, on top of the editor's own.
struct EnvironmentVariable {
  std::string name;
  std::string value;
};

// What to start: a program (a file, or a name looked for on PATH) with its arguments.
struct ProgramCall {
  std::string program;
  std::vector<std::string> arguments;
  std::vector<EnvironmentVariable> environment;
};

class ChildProcess {
 public:
  // Starts the program; nullptr, with `error` saying why, when it cannot.
  static std::unique_ptr<ChildProcess> start(const ProgramCall& call, std::string& error);
  ~ChildProcess();
  ChildProcess(const ChildProcess&) = delete;
  ChildProcess& operator=(const ChildProcess&) = delete;

  // Writes all of `bytes` to its standard input; false once it stopped reading.
  bool write(const std::string& bytes);
  // Waits for some of what it writes to its standard output (or error): how many bytes came into
  // `buffer`, 0 once it closed the stream (it ended).
  std::size_t read_output(char* buffer, std::size_t size);
  std::size_t read_errors(char* buffer, std::size_t size);
  // Ends it and everything it started; the reads then come back with 0. Safe to call twice.
  void stop();

  struct Handles;  // the system's own (child_process_posix.cpp, child_process_windows.cpp)

 private:
  explicit ChildProcess(std::unique_ptr<Handles> handles);
  std::unique_ptr<Handles> handles_;
};

// Whether a file can be started as a program here.
using ProgramTest = std::function<bool(const std::filesystem::path& file)>;

// Where `program` is: itself when it names a file (it has a folder in it), else the first folder of
// `search_path` (PATH's value, folders split by `separator`) holding it, tried with each of
// `extensions` (Windows' .exe, .cmd ...; "" first elsewhere). Empty when nowhere.
std::filesystem::path find_program_in(const std::string& program, const std::string& search_path, char separator,
                                      const std::vector<std::string>& extensions, const ProgramTest& runnable);
// The same on this machine, with its PATH (and PATHEXT on Windows).
std::filesystem::path find_program(const std::string& program);

}  // namespace redgem::agent
