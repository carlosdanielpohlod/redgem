#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/socket.hpp"

namespace redgem::net {

// The port games on a local network ask "who is hosting?" on.
inline constexpr std::uint16_t kDiscoveryPort = 7776;

// A game found on the local network.
struct NearbyGame {
  std::string address;       // where to join it
  std::uint16_t port = 0;
  std::string name;          // what its host called it
  int players = 0;           // machines in it, the host included
  float seen_ago = 0.0f;     // seconds since it last answered
};

// Lets games on the same network find each other with no address typed: a host answers whoever
// asks, and a machine looking for games asks everyone nearby (and itself, so two games on one
// machine find each other too) and collects the answers. Call update() every frame.
class LanDiscovery {
 public:
  // Starts answering searches: "`name`, on `game_port`". False when the discovery port cannot be
  // opened (the game still works, by its address).
  bool announce(const std::string& name, std::uint16_t game_port, std::uint16_t discovery_port = kDiscoveryPort);
  void set_players(int players) { players_ = players; }
  void stop_announcing();

  // Starts (or keeps) looking for games; they show up in games() as they answer.
  bool search(std::uint16_t discovery_port = kDiscoveryPort);
  void stop_searching();
  bool searching() const { return finder_.valid(); }
  // The games that answered lately, newest first heard of last.
  const std::vector<NearbyGame>& games() const { return games_; }

  void update(float seconds);

 private:
  void answer_searches();
  void collect_answers();
  void ask_around();

  Datagrams announcer_;   // a host's: bound to the discovery port
  Datagrams finder_;      // a searcher's: any port, answers come back to it
  std::string name_;
  std::uint16_t game_port_ = 0;
  std::uint16_t search_port_ = kDiscoveryPort;
  int players_ = 1;
  float since_asked_ = 0.0f;
  std::vector<NearbyGame> games_;
};

}  // namespace redgem::net
