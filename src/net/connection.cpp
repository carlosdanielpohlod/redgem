#include "net/connection.hpp"

#include <utility>

namespace redgem::net {

Connection::Connection(Socket socket, std::size_t pending_limit, bool established)
    : socket_(std::move(socket)),
      address_(socket_.peer_address()),
      pending_limit_(pending_limit),
      established_(established) {}

Connection Connection::accepted(Socket socket, std::size_t pending_limit) {
  return Connection(std::move(socket), pending_limit, true);
}

Connection Connection::calling(Socket socket) { return Connection(std::move(socket), kMaxPendingBytes, false); }

std::size_t Connection::pump() {
  taken_this_poll_ = 0;
  if (hung_up()) return 0;
  if (!established()) return 0;
  const std::size_t arrived = take_in();
  flush();
  return arrived;
}

bool Connection::established() {
  if (established_) return true;
  const IoStatus status = socket_.connect_status();
  if (status == IoStatus::Closed) hang_up();  // refused
  established_ = status == IoStatus::Ok;
  return established_;
}

std::size_t Connection::take_in() {
  // With that much of its talk still to handle, a machine's next words wait in the connection
  // (and it is not silent).
  if (incoming_.buffered() >= kMaxReadBytesPerPoll) {
    quiet_for_ = 0.0f;
    return 0;
  }
  std::vector<std::uint8_t> arrived;
  if (socket_.read(arrived, kMaxReadBytesPerPoll) == IoStatus::Closed) hang_up();
  if (!arrived.empty()) quiet_for_ = 0.0f;
  incoming_.append(arrived.data(), arrived.size());
  return arrived.size();
}

std::optional<nlohmann::json> Connection::next_message() {
  if (!takes_more()) return std::nullopt;
  const auto text = incoming_.next();
  if (!text) {
    if (incoming_.broken()) close();
    return std::nullopt;
  }
  ++taken_this_poll_;
  nlohmann::json message = nlohmann::json::parse(*text, nullptr, false);
  if (wire::well_formed(message)) return message;
  close();  // not one of ours
  return std::nullopt;
}

void Connection::send(const nlohmann::json& message) {
  if (hung_up()) return;
  // (Text that is not valid UTF-8 travels with the broken parts replaced, instead of not at all.)
  const std::vector<std::uint8_t> bytes = wire::packed(message);
  outgoing_.insert(outgoing_.end(), bytes.begin(), bytes.end());
  since_said_ = 0.0f;
  if (outgoing_.size() > pending_limit_) {
    close();  // it stopped listening: messages are not piled up for it forever
    return;
  }
  flush();
}

void Connection::flush() {
  if (!can_write()) return;
  if (socket_.write(outgoing_) == IoStatus::Closed) hang_up();
}

void Connection::keep_in_touch(float seconds) {
  quiet_for_ += seconds;
  since_said_ += seconds;
  if (quiet_for_ > kSilenceTimeoutSeconds) {
    close();  // gone without a goodbye
  } else if (since_said_ >= kPingEverySeconds) {
    send({{wire::kKind, wire::kPing}});
  }
}

void Connection::keep_waiting(float seconds) {
  waiting_for_ += seconds;
  if (waiting_for_ > kJoinTimeoutSeconds) close();  // connected and said nothing
}

void Connection::hang_up() {
  gone_ = true;
  socket_.close();
  outgoing_.clear();
}

void Connection::refuse(const std::string& reason) {
  send(wire::refusal(reason));
  flush();
  close();
}

void Connection::close() {
  hang_up();
  dropped_ = true;
  incoming_ = FrameBuffer();
}

}  // namespace redgem::net
