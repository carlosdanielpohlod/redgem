#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// What games on one local network say to find each other, and the list a machine keeps of the
// ones that answered. Anything can arrive on a port: all of it is checked here before it is used.
namespace redgem::net {

// A game found on the local network.
struct NearbyGame {
  std::string address;  // where to join it
  std::uint16_t port = 0;
  std::string name;       // what its host called it
  int players = 0;        // machines in it, the host included
  int protocol = 0;       // the version of the protocol it speaks (wire::kProtocolVersion)
  float seen_ago = 0.0f;  // seconds since it last answered
};

// What a machine looking for games asks everyone nearby.
inline constexpr const char* kSearchQuestion = "redgem: who is hosting?";

// Whether what a host heard from the address `from` is a search it answers.
bool search_to_answer(const std::string& message, const std::string& from);
// What a host answers a search with (in MessagePack): its game's name (as much of it as an answer
// holds), the port to join it on, how many are playing, and the version of the protocol it speaks.
std::string search_answer(const std::string& name, std::uint16_t game_port, int players);
// The game an answer to a search describes, heard from the address `from` (of another version of
// the protocol: who it is, and that version, whatever else it holds); nullopt for anything
// that is not a whole answer (anything can arrive on a port) and for one from an address that is
// not nearby (whoever knows the port could otherwise list games from anywhere).
std::optional<NearbyGame> game_from_answer(const std::string& message, const std::string& from);
// Puts a game that answered in the list: over what was known of it, or as a new one while there
// is room and its address has not filled its share. A game on this very machine answers twice
// (as this machine and by its address on the network): only the second, which the others see
// too, is kept.
void list_game(std::vector<NearbyGame>& games, const NearbyGame& game);

}  // namespace redgem::net
