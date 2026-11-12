#include "server/status_port.hpp"

#include <algorithm>

namespace redgem::server {

namespace {

// What may still be read of a request: one byte past the most, so that a longer one is seen as such.
std::size_t room_left(const std::vector<std::uint8_t>& received) {
  return received.size() > kMostStatusRequestBytes ? 0 : kMostStatusRequestBytes + 1 - received.size();
}

std::vector<std::uint8_t> bytes(const std::string& text) { return {text.begin(), text.end()}; }

bool too_old(float age) { return age > kMostStatusClientSeconds; }

// Whether an answer needs no more turns: all of it went out, or the other side is gone.
bool finished_sending(net::IoStatus status, const std::vector<std::uint8_t>& left) {
  return status == net::IoStatus::Closed || left.empty();
}

}  // namespace

bool StatusPort::open(const std::string& address, std::uint16_t port, std::string& error) {
  listener_ = net::Socket::listen_at(address, port, error);
  return listener_.valid();
}

void StatusPort::close() {
  clients_.clear();
  listener_.close();
}

void StatusPort::serve(float seconds, const Document& document) {
  if (!is_open()) return;
  const std::size_t waiting = clients_.size();
  take_newcomers();
  // (One taken this turn connected at some moment of it: it has waited no time yet.)
  for (std::size_t i = 0; i < clients_.size(); ++i) serve_client(clients_[i], i < waiting ? seconds : 0.0f, document);
  drop_done();
}

void StatusPort::take_newcomers() {
  net::Socket guest;
  while (clients_.size() < kMostStatusClients) {
    if (listener_.accept(guest) != net::IoStatus::Ok) return;
    clients_.push_back({std::move(guest), {}, {}, 0.0f, Stage::Listening});
  }
}

void StatusPort::serve_client(Client& client, float seconds, const Document& document) {
  client.age += seconds;
  if (client.stage == Stage::Listening) listen_to(client, document);
  if (client.stage == Stage::Answering) send_answer(client);
  if (too_old(client.age)) client.stage = Stage::Done;
}

void StatusPort::listen_to(Client& client, const Document& document) {
  const bool hung_up = client.socket.read(client.received, room_left(client.received)) == net::IoStatus::Closed;
  const std::string said(client.received.begin(), client.received.end());
  const StatusReply reply = status_reply(said, client.age, hung_up);
  if (reply != StatusReply::Wait) answer(client, reply, document);
}

void StatusPort::answer(Client& client, StatusReply reply, const Document& document) {
  client.stage = Stage::Answering;
  if (reply == StatusReply::Http) {
    client.answer = bytes(http_status_answer(std::string(client.received.begin(), client.received.end()), document()));
  } else if (reply == StatusReply::Plain) {
    client.answer = bytes(plain_status_answer(document()));
  } else {
    client.stage = Stage::Done;
  }
}

void StatusPort::send_answer(Client& client) {
  // (Either way the connection closes then.)
  if (finished_sending(client.socket.write(client.answer), client.answer)) client.stage = Stage::Done;
}

void StatusPort::drop_done() {
  clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
                                [](const Client& client) { return client.stage == Stage::Done; }),
                 clients_.end());
}

}  // namespace redgem::server
