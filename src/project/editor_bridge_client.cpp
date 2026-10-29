#include "project/editor_bridge.hpp"

#include <stdexcept>
#include <utility>

#include "project/editor_bridge_wire.hpp"

namespace redgem::tools {
namespace {
using namespace bridge;

net::Socket connect(const Address& address) {
  std::string error;
  auto socket = net::Socket::connect_to(net::kThisMachine, address.port, error);
  if (!socket.valid()) throw std::runtime_error("cannot connect to the editor");
  return socket;
}

class Exchange {
 public:
  Exchange(const Address& address, const std::filesystem::path& game, const std::string& name, const Json& args,
           EditorBridgeClock clock)
      : socket_(connect(address)),
        output_(encode({{"token", address.token},
                        {"game", std::filesystem::canonical(game).generic_u8string()},
                        {"name", name},
                        {"arguments", args}})),
        clock_(clock_or_steady(std::move(clock))) {
    if (output_.size() > kMaxRequest) throw std::runtime_error("editor request is too large");
  }
  Json run() {
    const auto started = clock_();
    while (!step()) {
      if (clock_() - started > kTimeout)
        throw std::runtime_error("editor request timed out; inspect the scene before retrying an edit");
      net::Socket::wait({{&socket_, !output_.empty()}}, kPollSeconds);
    }
    return response();
  }

 private:
  bool connected() {
    const auto connected = socket_.connect_status();
    if (connected == net::IoStatus::Closed) throw std::runtime_error("editor connection was refused");
    if (connected == net::IoStatus::WouldBlock) return false;
    return true;
  }
  bool step() {
    if (!connected()) return false;
    if (socket_.write(output_) == net::IoStatus::Closed) throw std::runtime_error("editor connection was lost");
    const auto read = socket_.read(input_, kMaxReply);
    if (input_.size() > kMaxReply) throw std::runtime_error("editor response is too large");
    if (complete(input_)) return true;
    if (read == net::IoStatus::Closed) throw std::runtime_error("editor closed the connection");
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
  Bytes input_, output_;
  EditorBridgeClock clock_;
};
}  // namespace

mcp::Json call_editor(const std::filesystem::path& game, const std::filesystem::path& connection,
                      const std::string& name, const mcp::Json& args, EditorBridgeClock clock) {
  try {
    const auto address = bridge::read_address(game, connection);
    return Exchange(address, game, name, args, std::move(clock)).run();
  } catch (const std::exception& error) {
    return mcp::tool_result(error.what(), true);
  }
}
}  // namespace redgem::tools
