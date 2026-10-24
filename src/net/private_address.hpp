#pragma once

#include <string>

namespace redgem::net {

// Whether an IPv4 address in numbers is this machine's or one of a network of its own rather
// than of the internet: loopback (127.x), private networks (10.x, 172.16.x to 172.31.x,
// 192.168.x), addresses a network gives itself (169.254.x) and carrier-grade NAT (100.64.x to
// 100.127.x). Where plain http:// may go, and where a game found nearby may be.
bool is_private_address(const std::string& numbers);

}  // namespace redgem::net
