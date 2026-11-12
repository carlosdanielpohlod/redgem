#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace redgem::net {

// What game.json's "servers" says: the relays and dedicated servers a game knows, each by the
// fingerprint of its certificate (what `redgem-relay --fingerprint` and `redgem-server <game>
// --fingerprint` print). A game that joins one of them checks that it is the one.
//   "servers": [
//     {"address": "play.example.com", "port": 7777, "fingerprint": "3F:A2:...:9C"},
//     {"address": "relay.example.com", "fingerprint": "..."}
//   ]
// Without "port", the fingerprint is the address's on every port.
inline constexpr const char* kServersKey = "servers";

struct ServerPin {
  std::string address;
  std::uint16_t port = 0;  // 0: any
  std::string digits;      // the whole fingerprint, as pin_digits gives it
};

// The entries of "servers" that can be used; what is wrong with the others goes to `warnings`
// (they are left out: a game that joins such a server checks nothing it does not know).
std::vector<ServerPin> read_server_pins(const nlohmann::json& servers, std::vector<std::string>& warnings);
// The fingerprint game.json gives for the server at `address` and `port` (digits); empty when it
// gives none.
std::string pinned_fingerprint(const std::vector<ServerPin>& pins, const std::string& address, std::uint16_t port);

}  // namespace redgem::net
