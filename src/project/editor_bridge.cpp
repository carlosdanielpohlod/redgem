#include "project/editor_bridge.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "project/editor_bridge_wire.hpp"

namespace redgem::tools {
namespace {
using namespace bridge;

net::Socket listener() {
  std::string error;
  auto socket = net::Socket::listen_local(0, error);
  if (!socket.valid()) throw std::runtime_error("cannot listen for local editor tools");
  return socket;
}

bool matches_game(const Address& address, const Json& asked) {
  if (!asked.value("game", Json()).is_string()) return false;
  const auto game = asked["game"].get<std::string>();
  if (game.find('\0') != std::string::npos) return false;
  return std::filesystem::equivalent(address.game, std::filesystem::u8path(game));
}

class Peer {
 public:
  explicit Peer(net::Socket socket) : socket_(std::move(socket)) {}
  bool pump(const Address& address, const mcp::Invoke& invoke) {
    if (Clock::now() - started_ > kTimeout) return false;
    if (!answered_ && !receive(address, invoke)) return false;
    if (!answered_) return true;
    if (socket_.write(output_) == net::IoStatus::Closed) return false;
    return !output_.empty();
  }

 private:
  bool receive(const Address& address, const mcp::Invoke& invoke) {
    if (socket_.read(input_, kMaxRequest) == net::IoStatus::Closed) return false;
    if (input_.size() > kMaxRequest) return false;
    if (!complete(input_)) return true;
    try {
      return answer(address, invoke);
    } catch (const std::exception&) {
      return false;  // malformed or unauthenticated peers get no protocol or credential detail
    }
  }
  bool answer(const Address& address, const mcp::Invoke& invoke) {
    const Json asked = parse(std::string(input_.begin(), input_.end()));
    if (!asked.is_object() || !same_token(address.token, asked.value("token", Json()))) return false;
    if (!matches_game(address, asked)) return false;
    if (!asked.value("name", Json()).is_string() || !asked.value("arguments", Json()).is_object()) return false;
    output_ = encode(invoke(asked["name"], asked["arguments"]));
    answered_ = true;
    return output_.size() <= kMaxReply;
  }
  net::Socket socket_;
  Bytes input_, output_;
  Clock::time_point started_ = Clock::now();
  bool answered_ = false;
};
}  // namespace

class EditorBridge::Impl {
 public:
  Impl(const std::filesystem::path& game, const std::filesystem::path& root)
      : listener_(listener()),
        address_{std::filesystem::canonical(game), listener_.local_port(), random_token()},
        file_(address_, root) {}
  const std::filesystem::path& path() const { return file_.path(); }
  void tick(const mcp::Invoke& invoke) {
    accept();
    peers_.erase(std::remove_if(peers_.begin(), peers_.end(), [&](Peer& peer) { return !peer.pump(address_, invoke); }),
                 peers_.end());
  }

 private:
  void accept() {
    if (peers_.size() == kMaxClients) return;
    net::Socket socket;
    if (listener_.accept(socket) == net::IoStatus::Ok) peers_.emplace_back(std::move(socket));
  }
  net::Socket listener_;
  Address address_;
  ConnectionFile file_;
  std::vector<Peer> peers_;
};

EditorBridge::EditorBridge(const std::filesystem::path& game, const std::filesystem::path& root)
    : impl_(std::make_unique<Impl>(game, root)) {}
EditorBridge::~EditorBridge() = default;
const std::filesystem::path& EditorBridge::connection_file() const { return impl_->path(); }
void EditorBridge::tick(const mcp::Invoke& invoke) { impl_->tick(invoke); }
}  // namespace redgem::tools
