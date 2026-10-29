// ChildProcess on Windows: CreateProcess with three pipes, inside a job object so that stopping it
// reaches what it started. Programs npm installs are batch files, which cmd.exe starts.

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOGDI   // raylib defines its own Rectangle
#define NOUSER  // ... and CloseWindow, ShowCursor
#include <windows.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>
#include <mutex>
#include <string>
#include <utility>

#include "editor/agent/child_process.hpp"

namespace redgem::agent {

struct ChildProcess::Handles {
  HANDLE process = nullptr;
  HANDLE job = nullptr;     // the process and everything it starts
  HANDLE input = nullptr;   // its standard input, written here
  HANDLE output = nullptr;  // its standard output, read here
  HANDLE errors = nullptr;  // its standard error, read here
  std::mutex stopping;
  bool stopped = false;
};

namespace {

// The program's three streams, in the order of the pipes made for them.
constexpr std::size_t kInput = 0;
constexpr std::size_t kOutput = 1;
constexpr std::size_t kErrors = 2;
constexpr std::size_t kStreams = 3;
constexpr UINT kStoppedCode = 1;  // what a stopped agent's exit code says
constexpr const wchar_t* kSpecialInArgument = L" \t\n\v\"";
constexpr const wchar_t* kShell = L"cmd.exe /d /s /c \"";  // what starts a batch file (closed by a quote)
constexpr const wchar_t* kBatchExtensions[] = {L".cmd", L".bat"};
constexpr wchar_t kQuote = L'"';
constexpr wchar_t kBackslash = L'\\';

// Both ends of one pipe: the child's (inherited by it) and the editor's.
struct PipeEnds {
  HANDLE child = nullptr;
  HANDLE editor = nullptr;
};

std::wstring wide(const std::string& utf8) { return std::filesystem::u8path(utf8).wstring(); }

void close_handle(HANDLE& handle) {
  if (handle != nullptr) CloseHandle(handle);
  handle = nullptr;
}

// One argument as the C runtime's parser of command lines reads it back: quoted when it has to be,
// with the backslashes before a quote doubled.
std::wstring quoted(const std::wstring& argument) {
  if (!argument.empty() && argument.find_first_of(kSpecialInArgument) == std::wstring::npos) return argument;
  std::wstring out(1, kQuote);
  std::size_t backslashes = 0;
  for (const wchar_t character : argument) {
    if (character == kBackslash) {
      ++backslashes;
      continue;
    }
    out.append(character == kQuote ? backslashes * 2 + 1 : backslashes, kBackslash);
    backslashes = 0;
    out += character;
  }
  out.append(backslashes * 2, kBackslash);
  return out + kQuote;
}

bool is_batch_file(const std::filesystem::path& program) {
  std::wstring extension = program.extension().wstring();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) { return std::towlower(c); });
  return std::any_of(std::begin(kBatchExtensions), std::end(kBatchExtensions),
                     [&](const wchar_t* batch) { return extension == batch; });
}

std::wstring command_line(const std::filesystem::path& program, const ProgramCall& call) {
  std::wstring line = quoted(program.wstring());
  for (const std::string& argument : call.arguments) line += L" " + quoted(wide(argument));
  return is_batch_file(program) ? kShell + line + kQuote : line;
}

bool same_name(const std::wstring& entry, const std::wstring& name) {
  if (entry.size() <= name.size() || entry[name.size()] != L'=') return false;
  return CompareStringOrdinal(entry.c_str(), static_cast<int>(name.size()), name.c_str(), static_cast<int>(name.size()),
                              TRUE) == CSTR_EQUAL;
}

bool overridden(const std::wstring& entry, const ProgramCall& call) {
  return std::any_of(call.environment.begin(), call.environment.end(),
                     [&](const EnvironmentVariable& variable) { return same_name(entry, wide(variable.name)); });
}

// The editor's environment block with the call's variables in place of the editor's own.
std::wstring environment_block(const ProgramCall& call) {
  std::wstring block;
  wchar_t* strings = GetEnvironmentStringsW();
  for (const wchar_t* entry = strings; *entry != L'\0'; entry += std::wcslen(entry) + 1) {
    if (!overridden(entry, call)) block += std::wstring(entry) + L'\0';
  }
  FreeEnvironmentStringsW(strings);
  for (const EnvironmentVariable& variable : call.environment) {
    block += wide(variable.name) + L"=" + wide(variable.value) + L'\0';
  }
  return block + L'\0';
}

// A pipe whose child end the started program inherits; `child_reads` for its standard input.
bool open_pipe(PipeEnds& ends, bool child_reads) {
  SECURITY_ATTRIBUTES inherited{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE read_end = nullptr;
  HANDLE write_end = nullptr;
  if (!CreatePipe(&read_end, &write_end, &inherited, 0)) return false;
  ends.child = child_reads ? read_end : write_end;
  ends.editor = child_reads ? write_end : read_end;
  SetHandleInformation(ends.editor, HANDLE_FLAG_INHERIT, 0);
  return true;
}

void close_pipes(std::array<PipeEnds, kStreams>& pipes) {
  for (PipeEnds& ends : pipes) {
    close_handle(ends.child);
    close_handle(ends.editor);
  }
}

// A job that ends every process in it when its last handle closes.
HANDLE job_that_ends_with_the_editor() {
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (job != nullptr) SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
  return job;
}

bool create(const std::filesystem::path& program, const ProgramCall& call, std::array<PipeEnds, kStreams>& pipes,
            PROCESS_INFORMATION& started) {
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = pipes[kInput].child;
  startup.hStdOutput = pipes[kOutput].child;
  startup.hStdError = pipes[kErrors].child;
  std::wstring line = command_line(program, call);
  std::wstring environment = environment_block(call);
  const DWORD flags = CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;
  return CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, flags, environment.data(), nullptr, &startup,
                        &started) != FALSE;
}

std::unique_ptr<ChildProcess::Handles> keep(const PROCESS_INFORMATION& started, HANDLE job,
                                            std::array<PipeEnds, kStreams>& pipes) {
  auto handles = std::make_unique<ChildProcess::Handles>();
  handles->process = started.hProcess;
  handles->job = job;
  handles->input = std::exchange(pipes[kInput].editor, nullptr);
  handles->output = std::exchange(pipes[kOutput].editor, nullptr);
  handles->errors = std::exchange(pipes[kErrors].editor, nullptr);
  close_pipes(pipes);  // (the child's ends: it holds its own now)
  return handles;
}

bool open_pipes(std::array<PipeEnds, kStreams>& pipes) {
  return open_pipe(pipes[kInput], true) && open_pipe(pipes[kOutput], false) && open_pipe(pipes[kErrors], false);
}

std::unique_ptr<ChildProcess::Handles> launch(const std::filesystem::path& program, const ProgramCall& call,
                                              std::string& error) {
  std::array<PipeEnds, kStreams> pipes{};
  PROCESS_INFORMATION started{};
  HANDLE job = job_that_ends_with_the_editor();
  if (job == nullptr || !open_pipes(pipes) || !create(program, call, pipes, started)) {
    error = "could not start " + program.u8string() + " (error " + std::to_string(GetLastError()) + ")";
    close_pipes(pipes);
    close_handle(job);
    return nullptr;
  }
  AssignProcessToJobObject(job, started.hProcess);
  ResumeThread(started.hThread);
  CloseHandle(started.hThread);
  return keep(started, job, pipes);
}

std::size_t read_some(HANDLE stream, char* buffer, std::size_t size) {
  DWORD got = 0;
  if (!ReadFile(stream, buffer, static_cast<DWORD>(size), &got, nullptr)) return 0;
  return got;
}

}  // namespace

ChildProcess::ChildProcess(std::unique_ptr<Handles> handles) : handles_(std::move(handles)) {}

ChildProcess::~ChildProcess() {
  stop();
  close_handle(handles_->output);
  close_handle(handles_->errors);
  close_handle(handles_->process);
  close_handle(handles_->job);
}

std::unique_ptr<ChildProcess> ChildProcess::start(const ProgramCall& call, std::string& error) {
  const std::filesystem::path program = find_program(call.program);
  if (program.empty()) {
    error = "'" + call.program + "' was not found";
    return nullptr;
  }
  auto handles = launch(program, call, error);
  if (!handles) return nullptr;
  return std::unique_ptr<ChildProcess>(new ChildProcess(std::move(handles)));
}

bool ChildProcess::write(const std::string& bytes) {
  std::size_t written = 0;
  while (written < bytes.size()) {
    DWORD put = 0;
    const DWORD wanted = static_cast<DWORD>(bytes.size() - written);
    if (!WriteFile(handles_->input, bytes.data() + written, wanted, &put, nullptr) || put == 0) return false;
    written += put;
  }
  return true;
}

std::size_t ChildProcess::read_output(char* buffer, std::size_t size) {
  return read_some(handles_->output, buffer, size);
}

std::size_t ChildProcess::read_errors(char* buffer, std::size_t size) {
  return read_some(handles_->errors, buffer, size);
}

void ChildProcess::stop() {
  const std::lock_guard<std::mutex> lock(handles_->stopping);
  if (handles_->stopped) return;
  handles_->stopped = true;
  close_handle(handles_->input);
  TerminateJobObject(handles_->job, kStoppedCode);
  WaitForSingleObject(handles_->process, INFINITE);
}

}  // namespace redgem::agent

#endif
