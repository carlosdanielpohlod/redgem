#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace redgem::net {

// Whether an IPv4 address in numbers is this machine's or one of a network of its own rather
// than of the internet: loopback (127.x), private networks (10.x, 172.16.x to 172.31.x,
// 192.168.x) and addresses a network gives itself (169.254.x). Where plain http:// may go, and
// where a game found nearby may be. (Not carrier-grade NAT, 100.64.x: that is the provider's
// network, shared with strangers.)
bool is_private_address(const std::string& numbers);
// An IPv4 address in numbers (four of them, dots between, nothing else) as one 32-bit number,
// its first number highest; nothing when it is no such address.
std::optional<std::uint32_t> ipv4_number(const std::string& numbers);
// The first number of an IPv4 address held as one 32-bit number.
int first_octet(std::uint32_t address);
// Whether an address is one where a cloud tells the machine it runs on about itself, keys
// included: no request goes there, whatever a game's scripts ask.
bool is_cloud_metadata_address(const std::string& numbers);

}  // namespace redgem::net
