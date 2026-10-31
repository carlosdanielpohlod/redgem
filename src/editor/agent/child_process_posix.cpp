// ChildProcess on Linux and the Mac: posix_spawn with three pipes, in a process group of its own
// so that stopping it reaches what it started.

#ifndef _WIN32

#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <mutex>
#include <string_view>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "editor/agent/child_process.hpp"

extern char** environ;  // NOLINT(readability-redundant-declaration): what the agent inherits

namespace redgem::agent {

constexpr std::size_t kWakeEnds = 2;  // the wake pipe's: read, write

struct ChildProcess::Handles {
  pid_t process = 0;
  int input = -1;   // its standard input, written here
  int output = -1;  // its standard output, read here
  int errors = -1;  // its standard error, read here
  // Written once by stop(): every read and write waiting on the agent comes back at once, even when
  // something that left its process group (a daemon it started) still holds its pipes open.
  std::array<int, kWakeEnds> wake{-1, -1};
  std::mutex stopping;
  bool stopped = false;
};

namespace {

constexpr int kSucceeded = 0;
constexpr int kWaitForever = -1;           // poll(): no time limit
constexpr std::size_t kWaitedStreams = 2;  // the stream, and the wake pipe
constexpr char kWakeByte = 0;
constexpr int kReadEnd = 0;
constexpr int kWriteEnd = 1;
constexpr std::size_t kPipeEnds = 2;
constexpr pid_t kOwnGroup = 0;  // setpgroup: a new group, numbered as the process
using Pipe = std::array<int, kPipeEnds>;
// The program's three streams, in the order of the pipes made for them.
constexpr std::size_t kInput = 0;
constexpr std::size_t kOutput = 1;
constexpr std::size_t kErrors = 2;
constexpr std::size_t kStreams = 3;
using Pipes = std::array<Pipe, kStreams>;

// The strings a call to the system takes: a list ending with null.
struct StringList {
  explicit StringList(std::vector<std::string> strings) : copies(std::move(strings)) {
    for (std::string& copy : copies) pointers.push_back(copy.data());
    pointers.push_back(nullptr);
  }
  std::vector<std::string> copies;
  std::vector<char*> pointers;
};

bool sets(std::string_view entry, const std::string& name) {
  return entry.size() > name.size() && entry.substr(0, name.size()) == name && entry[name.size()] == '=';
}

bool overridden(std::string_view entry, const std::vector<EnvironmentVariable>& added) {
  for (const EnvironmentVariable& variable : added) {
    if (sets(entry, variable.name)) return true;
  }
  return false;
}

// The editor's environment, with the call's variables in place of the editor's of the same names.
std::vector<std::string> environment_for(const ProgramCall& call) {
  std::vector<std::string> entries;
  for (char** entry = environ; *entry != nullptr; ++entry) {
    if (!overridden(*entry, call.environment)) entries.emplace_back(*entry);
  }
  for (const EnvironmentVariable& variable : call.environment) entries.push_back(variable.name + "=" + variable.value);
  return entries;
}

std::vector<std::string> arguments_for(const std::filesystem::path& program, const ProgramCall& call) {
  std::vector<std::string> arguments{program.string()};
  arguments.insert(arguments.end(), call.arguments.begin(), call.arguments.end());
  return arguments;
}

void close_end(int& end) {
  if (end >= 0) close(end);
  end = -1;
}

// Three pipes, the editor's ends kept from the child (and from any other program started later).
bool open_pipes(Pipes& pipes) {
  for (Pipe& ends : pipes) {
    if (pipe(ends.data()) != kSucceeded) return false;
  }
  fcntl(pipes[kInput][kWriteEnd], F_SETFD, FD_CLOEXEC);
  fcntl(pipes[kOutput][kReadEnd], F_SETFD, FD_CLOEXEC);
  fcntl(pipes[kErrors][kReadEnd], F_SETFD, FD_CLOEXEC);
  return true;
}

void close_all(Pipes& pipes) {
  for (Pipe& ends : pipes) {
    close_end(ends[kReadEnd]);
    close_end(ends[kWriteEnd]);
  }
}

// The child's ends of `pipes` as its standard streams.
void connect_streams(posix_spawn_file_actions_t& actions, const Pipes& pipes) {
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, pipes[kInput][kReadEnd], STDIN_FILENO);
  posix_spawn_file_actions_adddup2(&actions, pipes[kOutput][kWriteEnd], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, pipes[kErrors][kWriteEnd], STDERR_FILENO);
}

// A process group of its own, which stop() ends whole.
void own_group(posix_spawnattr_t& attributes) {
  posix_spawnattr_init(&attributes);
  posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
  posix_spawnattr_setpgroup(&attributes, kOwnGroup);
}

// Starts `program` with the child's ends of `pipes` as its streams; 0 when it cannot.
pid_t spawn(const std::filesystem::path& program, const ProgramCall& call, const Pipes& pipes) {
  posix_spawn_file_actions_t actions;
  connect_streams(actions, pipes);
  posix_spawnattr_t attributes;
  own_group(attributes);
  StringList arguments(arguments_for(program, call));
  StringList environment(environment_for(call));
  pid_t process = 0;
  const int failed = posix_spawn(&process, program.c_str(), &actions, &attributes, arguments.pointers.data(),
                                 environment.pointers.data());
  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attributes);
  return failed == kSucceeded ? process : 0;
}

// The editor's ends of the pipes of a started program, taken out of `pipes`.
// A pipe whose read end tells waits to give up, once something is written to it.
bool open_wake(Pipe& wake) {
  if (pipe(wake.data()) != kSucceeded) return false;
  for (const int end : wake) fcntl(end, F_SETFD, FD_CLOEXEC);
  return true;
}

bool open_all(Pipes& pipes, Pipe& wake) { return open_pipes(pipes) && open_wake(wake); }

std::unique_ptr<ChildProcess::Handles> keep_ends(pid_t process, Pipes& pipes, const Pipe& wake) {
  auto handles = std::make_unique<ChildProcess::Handles>();
  handles->process = process;
  handles->input = std::exchange(pipes[kInput][kWriteEnd], -1);
  handles->output = std::exchange(pipes[kOutput][kReadEnd], -1);
  handles->errors = std::exchange(pipes[kErrors][kReadEnd], -1);
  close_all(pipes);  // (the child's ends: it holds its own copies now)
  handles->wake = wake;
  return handles;
}

// Starts `program`; nullptr, with `error` saying why, when it cannot.
std::unique_ptr<ChildProcess::Handles> launch(const std::filesystem::path& program, const ProgramCall& call,
                                              std::string& error) {
  Pipes pipes{};
  for (Pipe& ends : pipes) ends = {-1, -1};
  Pipe wake{-1, -1};
  const pid_t process = open_all(pipes, wake) ? spawn(program, call, pipes) : 0;
  if (process != 0) return keep_ends(process, pipes, wake);
  error = "could not start " + program.string() + ": " + std::strerror(errno);
  close_all(pipes);
  for (int& end : wake) close_end(end);
  return nullptr;
}

// Waits until `stream` is ready for `events`; false when stop() woke the wait instead (or it failed).
// A call that failed only because a signal came first: tried again.
bool interrupted(long result) { return result < 0 && errno == EINTR; }

bool wait_for(int stream, short events, int wake) {
  std::array<pollfd, kWaitedStreams> waited{pollfd{stream, events, 0}, pollfd{wake, POLLIN, 0}};
  for (;;) {
    const int ready = poll(waited.data(), waited.size(), kWaitForever);
    if (interrupted(ready)) continue;
    return ready > 0 && waited[1].revents == 0;
  }
}

std::size_t read_some(int stream, int wake, char* buffer, std::size_t size) {
  if (!wait_for(stream, POLLIN, wake)) return 0;
  for (;;) {
    const ssize_t got = read(stream, buffer, size);
    if (got >= 0) return static_cast<std::size_t>(got);
    if (errno != EINTR) return 0;
  }
}

}  // namespace

ChildProcess::ChildProcess(std::unique_ptr<Handles> handles) : handles_(std::move(handles)) {}

ChildProcess::~ChildProcess() {
  stop();
  close_end(handles_->output);
  close_end(handles_->errors);
  for (int& end : handles_->wake) close_end(end);
}

std::unique_ptr<ChildProcess> ChildProcess::start(const ProgramCall& call, std::string& error) {
  const std::filesystem::path program = find_program(call.program);
  if (program.empty()) {
    error = "'" + call.program + "' was not found";
    return nullptr;
  }
  // (A write to an agent that ended is an error to handle, not a signal that ends the editor.)
  std::signal(SIGPIPE, SIG_IGN);
  auto handles = launch(program, call, error);
  if (!handles) return nullptr;
  return std::unique_ptr<ChildProcess>(new ChildProcess(std::move(handles)));
}

bool ChildProcess::write(const std::string& bytes) {
  std::size_t written = 0;
  while (written < bytes.size()) {
    if (!wait_for(handles_->input, POLLOUT, handles_->wake[kReadEnd])) return false;
    const ssize_t put = ::write(handles_->input, bytes.data() + written, bytes.size() - written);
    if (interrupted(put)) continue;
    if (put <= 0) return false;
    written += static_cast<std::size_t>(put);
  }
  return true;
}

std::size_t ChildProcess::read_output(char* buffer, std::size_t size) {
  return read_some(handles_->output, handles_->wake[kReadEnd], buffer, size);
}

std::size_t ChildProcess::read_errors(char* buffer, std::size_t size) {
  return read_some(handles_->errors, handles_->wake[kReadEnd], buffer, size);
}

void ChildProcess::stop() {
  const std::lock_guard<std::mutex> lock(handles_->stopping);
  if (handles_->stopped) return;
  handles_->stopped = true;
  const ssize_t woken = ::write(handles_->wake[kWriteEnd], &kWakeByte, sizeof(kWakeByte));
  static_cast<void>(woken);    // (a full wake pipe is already awake)
  close_end(handles_->input);  // (an agent that reads to the end of its input may leave by itself)
  kill(-handles_->process, SIGTERM);
  kill(-handles_->process, SIGKILL);
  int status = 0;
  waitpid(handles_->process, &status, 0);
}

}  // namespace redgem::agent

#endif
