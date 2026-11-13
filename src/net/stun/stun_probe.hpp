#pragma once

#include <cstddef>
#include <optional>

#include "net/stun/datagram_link.hpp"
#include "net/stun/stun_message.hpp"

namespace redgem::net::stun {

// How often a probe asks again while no answer came (datagrams get lost), and when it gives up.
inline constexpr float kProbeRetrySeconds = 0.5f;
inline constexpr float kProbeGiveUpSeconds = 5.0f;
// The most datagrams one update reads: what arrives faster waits for the next frame.
inline constexpr std::size_t kMostDatagramsPerUpdate = 16;

enum class ProbeState {
  Asking,  // no answer yet
  Found,   // the server said where it sees this machine: found()
  Failed   // no answer in time
};

// Asks a STUN server, over `link`, where it sees this machine: the public address and port its
// router gives the datagrams of this port. Call update() every frame until it is no longer Asking.
// Only the server's own answer to its own request counts: anything else that arrives is dropped.
class StunProbe {
 public:
  // `server` in numbers. `id` tells this probe's request apart (random_transaction_id()).
  StunProbe(DatagramLink& link, MappedAddress server, const TransactionId& id);

  void update(float seconds);
  ProbeState state() const { return state_; }
  // Where the server sees this machine, once Found.
  const MappedAddress& found() const { return found_; }

 private:
  void ask();
  void listen();
  // Whether an arrival is the server's answer to this request; it is kept as found_ when it is.
  bool take_answer(const Datagram& arrival);
  // Asks again, or gives up, as the time since the last request and since the first grows.
  void keep_time(float seconds);

  DatagramLink& link_;
  MappedAddress server_;
  TransactionId id_;
  std::string request_;
  float since_asked_ = kProbeRetrySeconds;  // the first update asks at once
  float waited_ = 0.0f;
  ProbeState state_ = ProbeState::Asking;
  MappedAddress found_;
};

}  // namespace redgem::net::stun
