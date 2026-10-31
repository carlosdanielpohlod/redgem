#include "app/encoder_process.hpp"

#include "app/recording.hpp"

// (A phone has no encoder program to start, and Android before API 28 has no posix_spawn: there
// a recording cannot start.)
#if defined(__ANDROID__)
namespace redgem {

EncoderProcess start_process(const std::vector<std::string>& /*arguments*/) { return {}; }

bool finish_process(EncoderProcess& process) {
  process.input = nullptr;
  return false;
}

bool runs(const std::vector<std::string>& /*arguments*/) { return false; }

ProcessEnd run_to_end(const std::vector<std::string>& /*arguments*/, const std::filesystem::path& /*output*/) {
  return {};
}

}  // namespace redgem
#else
#ifdef _WIN32
#include <cstdlib>
#else
#include <array>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;  // NOLINT(readability-redundant-declaration): what the encoder inherits
#endif

namespace redgem {
namespace {

constexpr int kSucceeded = 0;

#ifdef _WIN32
constexpr const char* kWriteBinary = "wb";
constexpr const char* kNowhere = " > NUL 2>&1";
// cmd.exe takes off the first and last quotes of a line that starts with one: they are doubled.
std::string for_the_shell(const std::string& command) { return "\"" + command + "\""; }
#else
constexpr const char* kWrite = "w";
constexpr const char* kNowhere = "/dev/null";
constexpr int kReadEnd = 0;
constexpr int kWriteEnd = 1;
constexpr std::size_t kPipeEnds = 2;
constexpr int kNoPermissions = 0;
constexpr mode_t kOwnerReadWrite = 0600;  // a run's output is its caller's alone

// The arguments as the system takes them: a list of strings, ending with null.
struct ArgumentList {
  explicit ArgumentList(const std::vector<std::string>& arguments) : copies(arguments) {
    for (std::string& copy : copies) pointers.push_back(copy.data());
    pointers.push_back(nullptr);
  }
  std::vector<std::string> copies;
  std::vector<char*> pointers;
};

// Starts `arguments` with what `actions` set up; 0 when it cannot.
pid_t spawn(const std::vector<std::string>& arguments, const posix_spawn_file_actions_t& actions) {
  ArgumentList list(arguments);
  pid_t process = 0;
  const int failed = posix_spawnp(&process, list.pointers.front(), &actions, nullptr, list.pointers.data(), environ);
  return failed == kSucceeded ? process : 0;
}

ProcessEnd end_of(pid_t process) {
  int status = 0;
  if (waitpid(process, &status, 0) != process) return {true, false, 0};
  if (WIFEXITED(status)) return {true, true, WEXITSTATUS(status)};
  return {true, false, WIFSIGNALED(status) ? WTERMSIG(status) : 0};
}

bool ended_well(pid_t process) {
  int status = 0;
  if (waitpid(process, &status, 0) != process) return false;
  return WIFEXITED(status) && WEXITSTATUS(status) == kSucceeded;
}
#endif

}  // namespace

#ifdef _WIN32
EncoderProcess start_process(const std::vector<std::string>& arguments) {
  return {_popen(for_the_shell(shell_command(arguments)).c_str(), kWriteBinary),
          0};  // NOLINT(bugprone-command-processor)
}

bool finish_process(EncoderProcess& process) {
  const int status = _pclose(process.input);
  process.input = nullptr;
  return status == kSucceeded;
}

bool runs(const std::vector<std::string>& arguments) {
  const std::string command = for_the_shell(shell_command(arguments) + kNowhere);
  return std::system(command.c_str()) == kSucceeded;  // NOLINT(bugprone-command-processor)
}

// (Windows says a crash with an exit code of its own, such as 0xC0000005: it reads as an exit.)
ProcessEnd run_to_end(const std::vector<std::string>& arguments, const std::filesystem::path& output) {
  const std::string command =
      for_the_shell(shell_command(arguments) + " < NUL > " + shell_command({output.string()}) + " 2>&1");
  const int code = std::system(command.c_str());  // NOLINT(bugprone-command-processor)
  return {true, true, code};
}
#else
EncoderProcess start_process(const std::vector<std::string>& arguments) {
  std::array<int, kPipeEnds> ends{};
  if (pipe(ends.data()) != kSucceeded) return {};
  fcntl(ends[kWriteEnd], F_SETFD, FD_CLOEXEC);  // (the encoder must not hold its own input open)
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, ends[kReadEnd], STDIN_FILENO);
  const pid_t process = spawn(arguments, actions);
  posix_spawn_file_actions_destroy(&actions);
  close(ends[kReadEnd]);
  if (process == 0) {
    close(ends[kWriteEnd]);
    return {};
  }
  return {fdopen(ends[kWriteEnd], kWrite), process};
}

bool finish_process(EncoderProcess& process) {
  std::fclose(process.input);
  process.input = nullptr;
  return ended_well(static_cast<pid_t>(process.id));
}

bool runs(const std::vector<std::string>& arguments) {
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  for (const int stream : {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}) {
    posix_spawn_file_actions_addopen(&actions, stream, kNowhere, stream == STDIN_FILENO ? O_RDONLY : O_WRONLY,
                                     kNoPermissions);
  }
  const pid_t process = spawn(arguments, actions);
  posix_spawn_file_actions_destroy(&actions);
  return process != 0 && ended_well(process);
}

ProcessEnd run_to_end(const std::vector<std::string>& arguments, const std::filesystem::path& output) {
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, kNowhere, O_RDONLY, kNoPermissions);
  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, output.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                                   kOwnerReadWrite);
  posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
  const pid_t process = spawn(arguments, actions);
  posix_spawn_file_actions_destroy(&actions);
  return process == 0 ? ProcessEnd{} : end_of(process);
}
#endif

}  // namespace redgem
#endif
