#include "editor/agent/agent_connection.hpp"

#include <array>
#include <chrono>
#include <utility>

#include "core/engine_thread.hpp"
#include "core/translations.hpp"

namespace redgem::agent {
namespace {

constexpr std::size_t kReadChunk = 64 * 1024;  // bytes asked of a stream at a time
constexpr char kLineEnd = '\n';
// How long the end of an agent's output waits for the end of its errors, which say why it ended.
constexpr auto kErrorsGrace = std::chrono::seconds(2);

// Starts a thread of the engine's running `work` of a connection.
template <typename Work>
std::thread engine_thread(Work work) {
  return std::thread([work] { run_engine_thread(work); });
}

}  // namespace

AgentConnection::AgentConnection(const ProgramCall& program, ClientSetup setup)
    : client_(std::move(setup), [this](const Json& message) { send(message); }),
      process_(ChildProcess::start(program, start_error_)) {
  if (!process_) {
    client_.fail(start_error_);
    return;
  }
  reader_ = engine_thread([this] { read_messages(); });
  error_reader_ = engine_thread([this] { read_errors(); });
  writer_ = engine_thread([this] { write_messages(); });
  client_.begin();
}

AgentConnection::~AgentConnection() {
  {
    const std::lock_guard<std::mutex> guard(lock_);
    closing_ = true;
  }
  outbox_ready_.notify_all();
  if (process_) process_->stop();  // (its streams close: the readers come back)
  for (std::thread* thread : {&reader_, &error_reader_, &writer_}) {
    if (thread->joinable()) thread->join();
  }
}

void AgentConnection::poll() {
  std::deque<Json> messages;
  bool ended = false;
  if (!take_inbox(messages, ended)) return;
  for (const Json& message : messages) client_.receive(message);
  if (ended) client_.fail(why_it_ended());
}

bool AgentConnection::take_inbox(std::deque<Json>& messages, bool& ended) {
  const std::lock_guard<std::mutex> guard(lock_);
  messages.swap(inbox_);
  ended = output_ended_ || unreadable_;
  return !messages.empty() || ended;
}

std::vector<std::string> AgentConnection::error_lines() const {
  const std::lock_guard<std::mutex> guard(lock_);
  return {error_lines_.begin(), error_lines_.end()};
}

std::string AgentConnection::why_it_ended() const {
  std::string why = unreadable_ ? tr("editor.agent.unreadable") : tr("editor.agent.ended");
  for (const std::string& line : error_lines()) why += kLineEnd + line;
  return why;
}

void AgentConnection::send(const Json& message) {
  {
    const std::lock_guard<std::mutex> guard(lock_);
    outbox_.push_back(to_line(message));
  }
  outbox_ready_.notify_one();
}

void AgentConnection::write_messages() {
  std::unique_lock<std::mutex> guard(lock_);
  for (;;) {
    outbox_ready_.wait(guard, [this] { return closing_ || !outbox_.empty(); });
    if (closing_) return;
    const std::string line = std::move(outbox_.front());
    outbox_.pop_front();
    guard.unlock();
    const bool written = process_->write(line);
    guard.lock();
    if (!written) return;  // (it stopped reading: the reader sees it end)
  }
}

void AgentConnection::read_messages() {
  LineReader lines;
  std::array<char, kReadChunk> buffer{};
  while (const std::size_t got = process_->read_output(buffer.data(), buffer.size())) {
    lines.feed(buffer.data(), got);
    while (const auto line = lines.next()) take_line(*line);
    if (lines.overflowed()) break;
  }
  std::unique_lock<std::mutex> guard(lock_);
  errors_ended_signal_.wait_for(guard, kErrorsGrace, [this] { return errors_ended_; });
  output_ended_ = true;
  unreadable_ = lines.overflowed();
}

void AgentConnection::take_line(const std::string& line) {
  Json message = Json::parse(line, nullptr, false);
  // (A line that is no message is the agent talking to itself: kept with its errors.)
  if (message.is_discarded()) return keep_error_line(line);
  const std::lock_guard<std::mutex> guard(lock_);
  inbox_.push_back(std::move(message));
}

void AgentConnection::read_errors() {
  LineReader lines;
  std::array<char, kReadChunk> buffer{};
  while (const std::size_t got = process_->read_errors(buffer.data(), buffer.size())) {
    lines.feed(buffer.data(), got);
    while (const auto line = lines.next()) keep_error_line(*line);
  }
  {
    const std::lock_guard<std::mutex> guard(lock_);
    errors_ended_ = true;
  }
  errors_ended_signal_.notify_all();
}

void AgentConnection::keep_error_line(const std::string& line) {
  if (line.empty()) return;
  const std::lock_guard<std::mutex> guard(lock_);
  error_lines_.push_back(line);
  if (error_lines_.size() > kKeptErrorLines) error_lines_.pop_front();
}

}  // namespace redgem::agent
