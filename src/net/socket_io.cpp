// Socket: what travels over a connection once it is made, and waiting for more of it.

#include "net/socket.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#include "net/socket_native.hpp"

namespace redgem::net {

namespace {

using namespace os;

constexpr std::size_t kReadChunkBytes = 16 * 1024;
constexpr float kMillisecondsPerSecond = 1000.0f;
constexpr float kLongestWaitSeconds = 3600.0f;  // a wait asked to last longer is cut to this

// Appends what one call takes from a connection to `into`. How many bytes that was: 0 when the
// other side hung up, negative when nothing came (see would_block).
std::ptrdiff_t read_chunk(NativeSocket socket, std::vector<std::uint8_t>& into) {
  char chunk[kReadChunkBytes];
  const auto count = recv(socket, chunk, sizeof(chunk), 0);
  if (count > 0) into.insert(into.end(), chunk, chunk + count);
  return static_cast<std::ptrdiff_t>(count);
}

IoStatus read_so_far(std::size_t got) { return got > 0 ? IoStatus::Ok : IoStatus::WouldBlock; }

// What a call that took nothing means: nothing more waiting right now, or a connection gone wrong.
IoStatus nothing_more(std::size_t got) { return would_block() ? read_so_far(got) : IoStatus::Closed; }

// Whether a wait of that length sleeps at all (one of no length, or of none that is a number, does not).
bool lasts(float seconds) { return seconds > 0.0f; }

// A wait's length as the system takes it: whole milliseconds, rounded up so that a short wait
// still sleeps, and never negative (which the system reads as "for ever").
int wait_milliseconds(float seconds) {
  if (!lasts(seconds)) return 0;
  return static_cast<int>(std::ceil(std::min(seconds, kLongestWaitSeconds) * kMillisecondsPerSecond));
}

bool can_be_watched(const Watched& watched) { return watched.socket != nullptr && watched.socket->valid(); }

// What the system is asked to watch a socket for.
short events_of(const Watched& watched) {
  return watched.sending ? static_cast<short>(kArrivals | kRoomToSend) : kArrivals;
}

}  // namespace

IoStatus Socket::read(std::vector<std::uint8_t>& into, std::size_t limit) {
  if (!valid()) return IoStatus::Closed;
  std::size_t got = 0;
  while (got < limit) {
    const std::ptrdiff_t count = read_chunk(native(handle_), into);
    if (count == 0) return IoStatus::Closed;  // hung up
    if (count < 0) return nothing_more(got);
    got += static_cast<std::size_t>(count);
  }
  return read_so_far(got);
}

IoStatus Socket::write(std::vector<std::uint8_t>& bytes) {
  if (!valid()) return IoStatus::Closed;
  while (!bytes.empty()) {
    const auto sent =
        send(native(handle_), reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), kSendFlags);
    if (sent <= 0) return later_or_closed();
    bytes.erase(bytes.begin(), bytes.begin() + sent);
  }
  return IoStatus::Ok;
}

bool Socket::wait(const std::vector<Watched>& sockets, float seconds) {
  std::vector<Waiting> waiting;
  for (const Watched& watched : sockets) {
    if (can_be_watched(watched)) waiting.push_back({native(watched.socket->handle_), events_of(watched), 0});
  }
  const int milliseconds = wait_milliseconds(seconds);
  if (waiting.empty()) {
    // With nothing to watch only the time can end the wait (and Windows refuses an empty list).
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return false;
  }
  return wait_native(waiting, milliseconds) > 0;
}

}  // namespace redgem::net
