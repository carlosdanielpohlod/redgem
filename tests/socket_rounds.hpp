#pragma once

#include <initializer_list>
#include <vector>

#include "net/network_session.hpp"
#include "net/socket/socket.hpp"

namespace redgem::test {

// The longest a test's round waits for one of its sockets to be ready before it goes on. What is
// sent on this computer is there to read at once or nearly, so a round this wait ends by the time
// had nothing on its way. Tests count rounds, never time them: the machines count the seconds
// each round tells them, so how fast the computer is changes nothing they decide.
inline constexpr float kSocketPatience = 0.001f;

// Waits until one of these sockets (a session's, a relay's, a connection's) has something to
// read, or room for what waits to be sent; kSocketPatience at most.
inline void wait_for_sockets(std::initializer_list<std::vector<net::Watched>> groups) {
  std::vector<net::Watched> all;
  for (const std::vector<net::Watched>& group : groups) all.insert(all.end(), group.begin(), group.end());
  net::Socket::wait(all, kSocketPatience);
}

// The same for the sockets of these sessions, and `also`.
inline void wait_for_sessions(const std::vector<net::NetworkSession*>& sessions, std::vector<net::Watched> also = {}) {
  for (const net::NetworkSession* session : sessions) {
    const std::vector<net::Watched> theirs = session->watched();
    also.insert(also.end(), theirs.begin(), theirs.end());
  }
  net::Socket::wait(also, kSocketPatience);
}

}  // namespace redgem::test
