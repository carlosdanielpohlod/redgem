#include "project/mcp_command.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "core/command_line.hpp"
#include "project/mcp_tools.hpp"
#include "project/tool_line.hpp"

namespace redgem::tools {
namespace {
constexpr const char* kReadOnly = "--read-only";
constexpr const char* kAllow = "--allow";
constexpr const char* kEditor = "--editor";
constexpr const char* kUsage =
    "usage: redgem mcp <game> [--editor connection.json] [--read-only] [--allow tool,tool]\n";

#ifdef _WIN32
int duplicate(int fd) { return _dup(fd); }
int replace_fd(int from, int to) { return _dup2(from, to); }
int descriptor(std::FILE* file) { return _fileno(file); }
void close_fd(int fd) { _close(fd); }
std::FILE* as_file(int fd) { return _fdopen(fd, "w"); }
#else
int duplicate(int fd) { return dup(fd); }
int replace_fd(int from, int to) { return dup2(from, to); }
int descriptor(std::FILE* file) { return fileno(file); }
void close_fd(int fd) { close(fd); }
std::FILE* as_file(int fd) { return fdopen(fd, "w"); }
#endif

// Native libraries must never write into the protocol channel, even via C stdio.
class Wire {
 public:
  Wire() {
    std::cout.flush();
    std::fflush(stdout);
    const int saved = duplicate(descriptor(stdout));
    if (saved < 0) throw std::runtime_error("cannot preserve protocol output");
    file_ = as_file(saved);
    if (file_ == nullptr) {
      close_fd(saved);
      throw std::runtime_error("cannot open protocol output");
    }
    redirect();
  }
  ~Wire() {
    std::cout.flush();
    std::fflush(stdout);
    replace_fd(descriptor(file_), descriptor(stdout));
    std::fclose(file_);
  }
  Wire(const Wire&) = delete;
  Wire& operator=(const Wire&) = delete;
  void send(const std::string& text) {
    if (std::fwrite(text.data(), 1, text.size(), file_) != text.size())
      throw std::runtime_error("protocol write failed");
    if (std::fputc('\n', file_) == EOF || std::fflush(file_) != 0) throw std::runtime_error("protocol write failed");
  }

 private:
  void redirect() {
    if (replace_fd(descriptor(stderr), descriptor(stdout)) >= 0) return;
    std::fclose(file_);
    throw std::runtime_error("cannot redirect native output");
  }
  std::FILE* file_ = nullptr;
};

std::set<std::string> allow_list(const std::string& text) {
  std::set<std::string> names;
  std::istringstream words(text);
  std::string word;
  while (std::getline(words, word, ',')) {
    if (word.empty()) throw std::invalid_argument("empty tool name in --allow");
    names.insert(word);
  }
  if (names.empty() || text.back() == ',') throw std::invalid_argument("--allow needs tool names");
  return names;
}

void validate_allow(const std::set<std::string>& names) {
  std::set<std::string> known;
  mcp::Options options;
  options.editor_connection = "live";
  for (const auto& tool : mcp::catalog(options)) known.insert(tool.at("name").get<std::string>());
  for (const auto& name : names) {
    if (known.count(name) == 0) throw std::invalid_argument("unknown tool in --allow: " + name);
  }
}

mcp::Options options(const ToolLine& line) {
  if (line.words.size() != 1) throw std::invalid_argument("mcp needs one game directory");
  mcp::Options result;
  result.game = std::filesystem::canonical(line.words.front());
  if (!std::filesystem::is_directory(result.game)) throw std::invalid_argument("game must be a directory");
  result.read_only = line.has(kReadOnly);
  if (const auto editor = line.value(kEditor)) result.editor_connection = *editor;
  if (const auto allow = line.value(kAllow)) result.allow = allow_list(*allow);
  validate_allow(result.allow);
  return result;
}

int serve_game(const mcp::Options& options) {
  mcp::Session session(mcp::catalog(options), [&](const std::string& name, const mcp::Json& args) {
    return mcp::invoke(options, name, args);
  });
  Wire wire;
  mcp::serve(std::cin, [&](const std::string& text) { wire.send(text); }, session);
  return kExitSuccess;
}

int run_requested(const ToolLine& line) {
  if (line.has(command_line::kHelpOption)) {
    std::cout << kUsage;
    return kExitSuccess;
  }
  try {
    return serve_game(options(line));
  } catch (const std::exception& error) {
    std::cerr << "mcp: " << error.what() << '\n';
    return kExitFailure;
  }
}
}  // namespace

int run_mcp_command(const std::vector<std::string>& args) {
  std::string problem;
  const auto line = read_tool_line(args, 0, {{kAllow, kEditor}, {kReadOnly, command_line::kHelpOption}}, problem);
  if (!line) {
    std::cerr << problem << '\n' << kUsage;
    return kExitUsage;
  }
  return run_requested(*line);
}
}  // namespace redgem::tools
