#include "net/connection.hpp"

#include <utility>

#include "core/log.hpp"
#include "net/secure/tls_session.hpp"
#include "net/traffic.hpp"

namespace redgem::net {

Connection::Connection() = default;
Connection::~Connection() = default;
Connection::Connection(Connection&& other) noexcept = default;
Connection& Connection::operator=(Connection&& other) noexcept = default;

Connection::Connection(Socket socket, std::size_t pending_limit, bool established)
    : socket_(std::move(socket)),
      address_(socket_.peer_address()),
      pending_limit_(pending_limit),
      established_(established),
      answering_(established) {}

Connection Connection::accepted(Socket socket, std::size_t pending_limit,
                                const std::shared_ptr<const TlsIdentity>& identity) {
  Connection connection(std::move(socket), pending_limit, true);
  if (identity) connection.encrypt(TlsSession::answering(identity, connection.tls_error_));
  return connection;
}

Connection Connection::calling(Socket socket, const TlsTrust& trust) {
  Connection connection(std::move(socket), kMaxPendingBytes, false);
  if (trust.encrypted()) connection.encrypt(TlsSession::calling(trust, connection.tls_error_));
  return connection;
}

void Connection::encrypt(std::unique_ptr<TlsSession> tls) {
  tls_ = std::move(tls);
  if (!tls_) close();  // TLS could not even start: nothing goes in the clear instead
}

std::size_t Connection::pump() {
  taken_this_poll_ = 0;
  if (hung_up()) return 0;
  if (!made()) return 0;
  const std::size_t arrived = take_in();
  flush();
  return arrived;
}

bool Connection::made() { return established() && secured(); }

bool Connection::established() {
  if (established_) return true;
  const IoStatus status = socket_.connect_status();
  if (status == IoStatus::Closed) hang_up();  // refused
  established_ = status == IoStatus::Ok;
  return established_;
}

bool Connection::secured() {
  if (!handshaking()) return true;
  const Handshake step = tls_->handshake(socket_);
  if (step == Handshake::Failed) hang_up();
  return step == Handshake::Done;
}

bool Connection::handshaking() const { return tls_ != nullptr && !tls_->done(); }

bool Connection::work_waiting() const {
  if (message_waiting()) return true;
  return tls_ != nullptr && tls_->holds_unread();
}

bool Connection::has_to_send() const {
  if (!outgoing_.empty()) return true;
  return tls_ != nullptr && tls_->handshake_sending();
}

std::string Connection::failure() const {
  if (!tls_error_.empty()) return tls_error_;
  return tls_ != nullptr ? tls_->failure() : std::string();
}

IoStatus Connection::read_some(std::vector<std::uint8_t>& into, std::size_t limit) {
  return tls_ != nullptr ? tls_->read(socket_, into, limit) : socket_.read(into, limit);
}

IoStatus Connection::write_out() {
  return tls_ != nullptr ? tls_->write(socket_, outgoing_) : socket_.write(outgoing_);
}

std::size_t Connection::take_in() {
  // With that much of its talk still to handle, a machine's next words wait in the connection
  // (and it is not silent).
  if (incoming_.buffered() >= kMaxReadBytesPerPoll) {
    quiet_for_ = 0.0f;
    return 0;
  }
  std::vector<std::uint8_t> arrived;
  if (read_some(arrived, kMaxReadBytesPerPoll) == IoStatus::Closed) hang_up();
  if (!arrived.empty()) quiet_for_ = 0.0f;
  incoming_.append(arrived.data(), arrived.size());
  count_received(arrived.size());
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
  return understood(wire::unpacked(*text));
}

std::optional<nlohmann::json> Connection::understood(const std::optional<nlohmann::json>& message) {
  if (!message) return not_ours();
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
  if (outgoing_.size() > pending_limit_) {
    close();       // it stopped listening: messages are not piled up for it forever
    lost_ = true;  // (its network stalled, most likely: what reconnecting is for too)
    return;
  }
  flush();
}

void Connection::flush() {
  if (!can_write()) return;
  const std::size_t waiting = outgoing_.size();
  const IoStatus status = write_out();
  count_sent(waiting - outgoing_.size());  // (the messages' bytes, before TLS adds its own)
  if (status == IoStatus::Closed) hang_up();
}

void Connection::keep_in_touch(float seconds) {
  quiet_for_ += seconds;
  since_said_ += seconds;
  if (quiet_for_ > kSilenceTimeoutSeconds) {
    close();  // gone without a goodbye
    lost_ = true;
  } else if (since_said_ >= kPingEverySeconds) {
    send({{wire::kKind, wire::kPing}});
  }
}

void Connection::keep_waiting(float seconds) {
  waiting_for_ += seconds;
  if (waiting_for_ > patience()) close();  // connected and said nothing (or never finished its handshake)
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
