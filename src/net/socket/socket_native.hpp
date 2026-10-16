#pragma once

// The operating systems' own socket calls, where POSIX and Windows spell them differently, and
// what Socket and Datagrams both build on them. Only the files that make those two include this:
// nothing else in the engine sees a system's socket header.

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#endif

#include "net/socket/socket.hpp"

namespace redgem::net::os {

#ifdef _WIN32
using NativeSocket = SOCKET;
using Length = int;
using Waiting = WSAPOLLFD;
inline constexpr int kSendFlags = 0;
// (Windows refuses a wait that also asks for urgent data, which POLLIN would: only the usual kind.)
inline constexpr short kArrivals = POLLRDNORM;
inline constexpr short kRoomToSend = POLLWRNORM;
#else
using NativeSocket = int;
using Length = socklen_t;
using Waiting = pollfd;
#ifdef MSG_NOSIGNAL
inline constexpr int kSendFlags = MSG_NOSIGNAL;  // a closed connection is an error to handle, not a signal
#else
inline constexpr int kSendFlags = 0;  // (the Mac has no such flag: each connection is told instead)
#endif
inline constexpr short kArrivals = POLLIN;
inline constexpr short kRoomToSend = POLLOUT;
#endif

inline constexpr std::intptr_t kInvalidSocket = -1;

inline NativeSocket native(std::intptr_t handle) { return static_cast<NativeSocket>(handle); }
// Whether the system gave a socket when asked for one.
inline bool is_socket(NativeSocket socket) { return static_cast<std::intptr_t>(socket) >= 0; }

// Has to be called before any other socket call (Windows starts its sockets on the first one).
void start_sockets();
// Whether the last call did nothing only because there was nothing to do yet.
bool would_block();
// What a call that did nothing means: try again later, or the connection is gone.
IoStatus later_or_closed();
std::string last_error();
void close_native(NativeSocket socket);
// Reads and drops what arrived on a connection and was never read. A connection closed with that
// still in it ends with a reset instead of a goodbye, and on Windows a reset makes the other side
// lose what it had not read yet of what this one said last.
void discard_unread(NativeSocket socket);
bool make_non_blocking(NativeSocket socket);
// Whether a connection in the making has an answer yet: 1 yes, 0 not yet, negative on an error.
int connection_answered(NativeSocket socket);
// Sleeps until one of `waiting` has what it waits for, `milliseconds` at most. How many have;
// negative when the system refused (or a signal cut the sleep short).
int wait_native(std::vector<Waiting>& waiting, int milliseconds);

void set_option(NativeSocket socket, int level, int option);
// Makes sending on a connection the other side closed an error to handle, not a signal that ends
// the program, where the send itself cannot say so (kSendFlags).
void no_signal_on_closed(NativeSocket socket);
// A socket of the given kind that never waits; kInvalidSocket, with `error` set, when there is none to have.
std::intptr_t open_non_blocking(int type, int protocol, std::string& error);
// Takes a port of this machine, on every address it has.
bool bind_to_port(NativeSocket socket, std::uint16_t port);
// The port a socket got; 0 when it has none.
std::uint16_t bound_port(NativeSocket socket);
std::string in_numbers(const sockaddr_in& address);

}  // namespace redgem::net::os
