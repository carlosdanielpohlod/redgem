#include "project/editor_bridge.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "project/editor_bridge_wire.hpp"

namespace redgem::tools {
namespace {
using namespace bridge;

// No editor took the request: what ends a try before anything secret was sent.
struct Missed : std::runtime_error {
  Missed(EditorMiss why, const std::string& text) : std::runtime_error(text), miss(why) {}
  EditorMiss miss;
};
constexpr const char* kNotRunning =
    "the editor is not running (its connection was refused). Start it with `redgem <game> --editor-bridge` and "
    "restart this server with its new connection file, or use --editor auto to find it on every call";
constexpr const char* kUnproven =
    "the program at the editor's port timed out or failed to prove it is this game's editor (a stale connection "
    "file?); nothing was sent to it. Restart the editor with --editor-bridge, or use --editor auto";

net::Socket connect(const Address& address) {
  std::string error;
  auto socket = net::Socket::connect_to(net::kThisMachine, address.port, error);
  if (!socket.valid()) throw Missed(EditorMiss::Refused, kNotRunning);
  return socket;
}

Address address_of(const std::filesystem::path& game, const std::filesystem::path& connection) {
  try {
    return bridge::read_address(game, connection);
  } catch (const std::exception& problem) {
    throw Missed(EditorMiss::Unproven, std::string(problem.what()) + "; " + kNotRunning);
  }
}

// Hello, proof, then the request and its answer. Until the editor proved itself a failure is a
// miss (nothing but a nonce went out); after that it is the request's failure, never retried.
class Exchange {
 public:
  Exchange(const Address& address, const std::filesystem::path& game, const std::string& name, const Json& args,
           EditorBridgeClock clock)
      : socket_(connect(address)),
        token_(address.token),
        nonce_(random_token()),
        request_(encode({{"token", address.token},
                         {"game", std::filesystem::canonical(game).generic_u8string()},
                         {"name", name},
                         {"arguments", args}})),
        output_(encode({{"hello", nonce_}})),
        clock_(clock_or_steady(std::move(clock))) {
    if (request_.size() > kMaxRequest) throw std::runtime_error("editor request is too large");
  }
  Json run() {
    const auto started = clock_();
    while (!step()) {
      check_deadline(clock_() - started);
      net::Socket::wait({{&socket_, !output_.empty()}}, kPollSeconds);
    }
    return response();
  }

 private:
  void check_deadline(Clock::duration waited) const {
    if (!proven_ && waited > kHandshakeTimeout) throw Missed(EditorMiss::Unproven, kUnproven);
    if (waited > kTimeout)
      throw std::runtime_error("editor request timed out; inspect the scene before retrying an edit");
  }
  // A failure: a miss before the proof, the request's own after it.
  [[noreturn]] void fail(const char* after_proof) const {
    if (!proven_) throw Missed(EditorMiss::Unproven, kUnproven);
    throw std::runtime_error(after_proof);
  }
  // Only the first answer of the connection attempt can be a refusal: a reset once connected (even
  // after the request went out) is the request's failure, never a sign that no editor is there.
  bool connected() {
    if (connected_) return true;
    const auto status = socket_.connect_status();
    if (status == net::IoStatus::Closed) throw Missed(EditorMiss::Refused, kNotRunning);
    connected_ = status == net::IoStatus::Ok;
    return connected_;
  }
  bool step() {
    if (!connected()) return false;
    if (socket_.write(output_) == net::IoStatus::Closed) fail("editor connection was lost");
    const auto read = socket_.read(input_, kMaxReply);
    if (input_.size() > kMaxReply) fail("editor response is too large");
    if (complete(input_)) return proven_ || prove();
    if (read == net::IoStatus::Closed) fail("editor closed the connection");
    return false;
  }
  // Checks the editor's proof; then the request goes out. False: its answer is still to come.
  bool prove() {
    Json answer;
    try {
      answer = parse(take_line(input_));
    } catch (const std::exception&) {
      throw Missed(EditorMiss::Unproven, kUnproven);
    }
    const Json proof = answer.is_object() ? answer.value("proof", Json()) : Json();
    if (!same_token(server_proof(token_, nonce_), proof)) throw Missed(EditorMiss::Unproven, kUnproven);
    proven_ = true;
    output_ = request_;
    return false;
  }
  Json response() const {
    try {
      const Json reply = parse(std::string(input_.begin(), input_.end()));
      if (reply.is_object() && reply.value("content", Json()).is_array()) return reply;
    } catch (const std::exception&) {
      // Never include wire data (or a credential echoed by an unrelated listener) in diagnostics.
      throw std::runtime_error("invalid editor response");
    }
    throw std::runtime_error("invalid editor response");
  }
  net::Socket socket_;
  std::string token_, nonce_;
  Bytes request_, input_, output_;
  EditorBridgeClock clock_;
  bool connected_ = false;
  bool proven_ = false;
};
}  // namespace

EditorAttempt try_editor(const std::filesystem::path& game, const std::filesystem::path& connection,
                         const std::string& name, const mcp::Json& args, EditorBridgeClock clock) {
  try {
    return {Exchange(address_of(game, connection), game, name, args, std::move(clock)).run(), EditorMiss::None};
  } catch (const Missed& missed) {
    return {std::nullopt, missed.miss};
  } catch (const std::exception& error) {
    return {mcp::tool_result(error.what(), true), EditorMiss::None};
  }
}

mcp::Json call_editor(const std::filesystem::path& game, const std::filesystem::path& connection,
                      const std::string& name, const mcp::Json& args, EditorBridgeClock clock) {
  try {
    return Exchange(address_of(game, connection), game, name, args, std::move(clock)).run();
  } catch (const std::exception& error) {
    return mcp::tool_result(error.what(), true);
  }
}

void forget_connection(const std::filesystem::path& connection) {
  std::error_code ignored;
  std::filesystem::remove_all(connection.parent_path(), ignored);
}

std::vector<std::filesystem::path> editor_connections(const std::filesystem::path& game,
                                                      const std::filesystem::path& private_root) {
  std::vector<std::filesystem::path> found = bridge::connection_files(private_root);
  found.erase(std::remove_if(found.begin(), found.end(),
                             [&](const std::filesystem::path& file) { return !bridge::belongs_to(game, file); }),
              found.end());
  return found;
}
}  // namespace redgem::tools
