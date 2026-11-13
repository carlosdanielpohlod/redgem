#pragma once

// A stand-in for the network between a machine and the one it calls: it takes connections on a
// port of its own and carries every byte to and from `target`, unchanged (TLS goes through it
// whole). A test cuts the connections it carries at will, as a network that fails would (with a
// reset, which both sides hear), and may keep the next calls from getting through for a while.

#include <cstdint>
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
    take_calls();
    for (Line& line : lines_) carry(line);
  }

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

 private:
  struct Line {
    net::Socket near, far;
    std::vector<std::uint8_t> to_far, to_near;
    bool far_ready = false;
  };

  static constexpr const char* kLocalhost = "127.0.0.1";
  static constexpr std::size_t kReadBytes = 64 * 1024;

  void take_calls() {
    for (net::Socket caller; listener_.accept(caller) == net::IoStatus::Ok;) {
      if (blocked_) {
        caller.reset();
        continue;
      }
      std::string error;
      lines_.push_back({std::move(caller), net::Socket::connect_to(kLocalhost, target_, error), {}, {}, false});
    }
  }

  // Whether the call on to `target` is through; one refused hangs up on the caller.
  static bool through(Line& line) {
    const net::IoStatus status = line.far.connect_status();
    if (status == net::IoStatus::Closed) line.near.close();
    return status == net::IoStatus::Ok;
  }

  static void carry(Line& line) {
    if (!line.far_ready) line.far_ready = through(line);
    if (!line.far_ready) return;
    // (What one side hung up on, the other hears hung up too, once what it said went through.)
    const bool near_gone = line.near.read(line.to_far, kReadBytes) == net::IoStatus::Closed;
    const bool far_gone = line.far.read(line.to_near, kReadBytes) == net::IoStatus::Closed;
    line.far.write(line.to_far);
    line.near.write(line.to_near);
    if (near_gone) line.far.close();
    if (far_gone) line.near.close();
  }

  net::Socket listener_;
  std::uint16_t target_ = 0;
  std::vector<Line> lines_;
  bool blocked_ = false;
};

}  // namespace redgem::test
