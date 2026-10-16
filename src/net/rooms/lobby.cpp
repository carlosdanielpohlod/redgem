#include "net/rooms/lobby.hpp"

#include <algorithm>

namespace redgem::net {

using namespace wire;

namespace {

// Machines that connected and have not said what they want yet: how many from one address, and
// how many in all. They hold no place in a room, so whoever connects and keeps quiet keeps
// nobody out but the next few from its own address.
constexpr int kMaxWaitingPerAddress = 4;
constexpr std::size_t kMaxWaiting = 64;

// A newcomer's first words must ask for a game. On a dedicated server the rooms are the
// server's to open: asking to open one, as on a relay, is no way in.
bool asks_for_a_game(const nlohmann::json& message) {
  return message[kKind] == kHello && message.value(kRoom, std::string()) != kNewRoom;
}

}  // namespace

bool Lobby::open(std::uint16_t port, std::string& error, int connections_per_address) {
  connections_per_address_ = connections_per_address;
  listener_ = Socket::listen_on(port, error);
  if (!listener_.valid()) return false;
  port_ = listener_.local_port();
  return true;
}

std::vector<Arrival> Lobby::poll(float seconds, const AddressCount& in_rooms) {
  if (!listener_.valid()) return {};
  guessers_.forgive(seconds);
  accept_newcomers(in_rooms);
  for (Newcomer& newcomer : newcomers_) hear(newcomer, seconds);
  return take_arrivals();
}

void Lobby::accept_newcomers(const AddressCount& in_rooms) {
  for (Socket guest; listener_.accept(guest) == IoStatus::Ok;) {
    if (!room_to_wait(guest.peer_address(), in_rooms)) continue;  // hung up on
    newcomers_.push_back({Connection::accepted(std::move(guest), kMaxPendingPerServed), std::nullopt, nullptr});
  }
}

bool Lobby::room_to_wait(const std::string& address, const AddressCount& in_rooms) const {
  const int waiting = waiting_from(address);
  // Not when full, kept out for guessing, or with too many from one place already.
  return newcomers_.size() < kMaxWaiting && !guessers_.keeps_out(address) && waiting < kMaxWaitingPerAddress &&
         waiting + in_rooms(address) < connections_per_address_;
}

int Lobby::waiting_from(const std::string& address) const {
  return static_cast<int>(std::count_if(newcomers_.begin(), newcomers_.end(), [&address](const Newcomer& newcomer) {
    return newcomer.connection.address() == address;
  }));
}

void Lobby::hear(Newcomer& newcomer, float seconds) {
  Connection& connection = newcomer.connection;
  connection.keep_waiting(seconds);
  connection.pump();
  const auto message = connection.next_message();
  if (!message) return;
  if (!asks_for_a_game(*message)) {
    connection.close();
    return;
  }
  newcomer.wants = message->value(kRoom, std::string());
  newcomer.auth = message->value(kAuth, nlohmann::json());
}

std::vector<Arrival> Lobby::take_arrivals() {
  std::vector<Arrival> arrivals;
  for (Newcomer& newcomer : newcomers_) {
    if (newcomer.ready()) {
      arrivals.push_back({std::move(newcomer.connection), newcomer.wants.value_or(""), std::move(newcomer.auth)});
    }
  }
  newcomers_.erase(
      std::remove_if(newcomers_.begin(), newcomers_.end(), [](const Newcomer& newcomer) { return newcomer.done(); }),
      newcomers_.end());
  return arrivals;
}

void Lobby::turn_away_guess(Arrival& arrival) {
  guessers_.note_wrong_code(arrival.connection.address());
  arrival.connection.close();
}

void Lobby::close() {
  listener_.close();
  newcomers_.clear();
}

}  // namespace redgem::net
