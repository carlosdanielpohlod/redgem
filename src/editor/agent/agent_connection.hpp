#pragma once

// A coding agent running beside the editor: its program, the threads that read and write its
// streams, and the conversation (AgentClient). The editor's frame never waits on the agent: what it
// sent is taken in by poll(), once a frame, and what is sent to it is written by a thread of its own.

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "editor/agent/agent_client.hpp"
#include "editor/agent/child_process.hpp"

namespace redgem::agent {

// How many of the last lines the agent wrote to its standard error are kept, to show when it fails.
inline constexpr std::size_t kKeptErrorLines = 40;

class AgentConnection {
 public:
  // Starts `program` and sends initialize. When it cannot be started the connection has failed,
  // with why in the client's error.
  AgentConnection(const ProgramCall& program, ClientSetup setup);
  ~AgentConnection();
  AgentConnection(const AgentConnection&) = delete;
  AgentConnection& operator=(const AgentConnection&) = delete;

  // Takes in what the agent sent since the last call (on the editor's thread).
  void poll();
  AgentClient& client() { return client_; }
  const AgentClient& client() const { return client_; }
  // The last lines of the agent's standard error (its own messages: a login it wants, a crash).
  std::vector<std::string> error_lines() const;

 private:
  void send(const Json& message);
  void read_messages();
  void take_line(const std::string& line);
  void read_errors();
  void write_messages();
  void keep_error_line(const std::string& line);
  bool take_inbox(std::deque<Json>& messages, bool& ended);
  std::string why_it_ended() const;

  AgentClient client_;
  std::string start_error_;  // why the program did not start (set before process_ is)
  std::unique_ptr<ChildProcess> process_;

  mutable std::mutex lock_;  // guards everything below, which the threads share
  std::condition_variable outbox_ready_;
  std::condition_variable errors_ended_signal_;
  std::deque<std::string> outbox_;  // lines to write
  std::deque<Json> inbox_;          // messages read
  std::deque<std::string> error_lines_;
  bool output_ended_ = false;  // the agent closed its output (it ended)
  bool unreadable_ = false;    // it wrote something that is no message
  bool errors_ended_ = false;  // it closed its standard error
  bool closing_ = false;

  std::thread reader_;
  std::thread error_reader_;
  std::thread writer_;
};

}  // namespace redgem::agent
