#include "net/connection.hpp"

#include <cstdio>
#include <utility>

#include "core/log.hpp"

namespace redgem::net {

Connection::Connection(Socket socket, std::size_t pending_limit, bool established)
    : socket_(std::move(socket)),
      address_(socket_.peer_address()),
      pending_limit_(pending_limit),
      established_(established),
      answering_(established) {}

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
    if (incoming_.broken()) { std::fprintf(stderr, "DBG broken\n"); close(); }
    return std::nullopt;
  }
  ++taken_this_poll_;
  return understood(wire::unpacked(*text));
}

std::optional<nlohmann::json> Connection::understood(const std::optional<nlohmann::json>& message) {
  if (!message) { std::fprintf(stderr, "DBG not ours\n"); return not_ours(); }
  if (first_of_another_protocol(*message)) return another_protocol(wire::protocol_of(*message));
  heard_ = true;
  if (wire::well_formed(*message)) return message;
  return not_ours();
}

std::optional<nlohmann::json> Connection::not_ours() {
  close();
  return std::nullopt;
}

std::optional<nlohmann::json> Connection::another_protocol(int theirs) {
  heard_ = true;
  if (!answering_) return wire::refusal_of_protocol(theirs);
  const std::string why = wire::protocol_mismatch(theirs);
  log::warning("turned away the machine at " + address_ + ": " + why);
  refuse(why);
  return std::nullopt;
}

void Connection::send(const nlohmann::json& message) {
  // (Text that is not valid UTF-8 travels with the broken parts replaced, instead of not at all.)
  send_packed(wire::packed(message));
}

void Connection::send_packed(const std::vector<std::uint8_t>& message) {
  if (hung_up()) return;
  outgoing_.insert(outgoing_.end(), message.begin(), message.end());
  since_said_ = 0.0f;
  static std::size_t dbg_most = 65536;
  if (outgoing_.size() > dbg_most) { dbg_most = outgoing_.size() * 2; std::fprintf(stderr, "DBG outgoing %zu limit %zu\n", outgoing_.size(), pending_limit_); }
  if (outgoing_.size() > pending_limit_) {
    std::fprintf(stderr, "DBG pending %zu\n", outgoing_.size());
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
    std::fprintf(stderr, "DBG silence\n");
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
  std::fprintf(stderr, "DBG hang_up %s answering %d dropped %d\n", address_.c_str(), (int)answering_, (int)dropped_);
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
