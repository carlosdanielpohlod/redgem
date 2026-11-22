#pragma once

// A stand-in for the network between a machine and the one it calls: it takes connections on a
// port of its own and carries every byte to and from `target`, unchanged (TLS goes through it
// whole). A test cuts the connections it carries at will, as a network that fails would (with a
// reset, which both sides hear), and may keep the next calls from getting through for a while.
// It may also hold what it carries a number of its turns each way, as a network far away would.

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "net/socket/socket.hpp"

namespace redgem::test {

class Middleman {
 public:
  explicit Middleman(std::uint16_t target) : target_(target) {
    std::string error;
    listener_ = net::Socket::listen_on(0, error);
  }

  bool valid() const { return listener_.valid(); }
  std::uint16_t port() const { return listener_.local_port(); }

  // One turn: new calls are taken (or hung up on, while blocked), and bytes carried both ways.
  void pump() {
    ++turn_;
    take_calls();
    for (Line& line : lines_) carry(line);
  }

  // What comes in from now on goes on `turns` of its turns later, each way.
  void delay(int turns) { delay_ = turns; }

  // Every connection carried now fails at once, with a reset on both sides.
  void cut() {
    for (Line& line : lines_) {
      line.near.reset();
      line.far.reset();
    }
    lines_.clear();
  }

  // While blocked, calls are taken and hung up on at once: the other side is out of reach.
  void block(bool blocked) { blocked_ = blocked; }
  std::size_t carried() const { return lines_.size(); }

  // What a wait for its next turn watches: calls coming in, and both ends of every line still
  // carrying (the far one for its call going through, either one while bytes wait for it). A line
  // one side of which is closed has nothing more to carry: its other end, hung up on, would end
  // every wait at once.
  std::vector<net::Watched> watched() const {
    std::vector<net::Watched> watched{{&listener_}};
    for (const Line& line : lines_) {
      if (!carrying(line)) continue;
      watched.push_back({&line.near, !line.to_near.empty()});
      watched.push_back({&line.far, !line.far_ready || !line.to_far.empty()});
      // (Bytes held back wait for turns, not for a socket: nothing here would tell when they are due.)
    }
    return watched;
  }

 private:
  // Bytes read, and the turn they go on at.
  struct Held {
    long due = 0;
    std::vector<std::uint8_t> bytes;
  };
  struct Line {
    net::Socket near, far;
    std::vector<std::uint8_t> to_far, to_near;
    bool far_ready = false;
    std::deque<Held> held_far, held_near;
  };

  static constexpr const char* kLocalhost = "127.0.0.1";

  static bool carrying(const Line& line) { return line.near.valid() && line.far.valid(); }
  static constexpr std::size_t kReadBytes = 64 * 1024;

  void take_calls() {
    for (net::Socket caller; listener_.accept(caller) == net::IoStatus::Ok;) {
      if (blocked_) {
        caller.reset();
        continue;
      }
      std::string error;
      lines_.push_back({std::move(caller), net::Socket::connect_to(kLocalhost, target_, error), {}, {}, false, {}, {}});
    }
  }

  // Whether the call on to `target` is through; one refused hangs up on the caller.
  static bool through(Line& line) {
    const net::IoStatus status = line.far.connect_status();
    if (status == net::IoStatus::Closed) line.near.close();
    return status == net::IoStatus::Ok;
  }

  // Reads what `from` sent, to go on `delay_` turns from now; what is due joins `ready`.
  void hold(net::Socket& from, std::deque<Held>& held, std::vector<std::uint8_t>& ready, bool& gone) const {
    std::vector<std::uint8_t> read;
    gone = from.read(read, kReadBytes) == net::IoStatus::Closed;
    if (!read.empty()) held.push_back({turn_ + delay_, std::move(read)});
    for (; !held.empty() && held.front().due <= turn_; held.pop_front()) {
      ready.insert(ready.end(), held.front().bytes.begin(), held.front().bytes.end());
    }
  }

  void carry(Line& line) const {
    if (!line.far_ready) line.far_ready = through(line);
    if (!line.far_ready) return;
    // (What one side hung up on, the other hears hung up too, once what it said went through.)
    bool near_gone = false;
    bool far_gone = false;
    hold(line.near, line.held_far, line.to_far, near_gone);
    hold(line.far, line.held_near, line.to_near, far_gone);
    line.far.write(line.to_far);
    line.near.write(line.to_near);
    if (near_gone) line.far.close();
    if (far_gone) line.near.close();
  }

  net::Socket listener_;
  std::uint16_t target_ = 0;
  std::vector<Line> lines_;
  bool blocked_ = false;
  long turn_ = 0;
  int delay_ = 0;
};

}  // namespace redgem::test
