// A request's connection as it is (http://), and the waiting every connection does.

#include "net/http/http_stream.hpp"

#include <algorithm>
#include <sstream>

namespace redgem::net {

namespace {

constexpr std::size_t kReadBytes = 64 * 1024;  // what one receive takes at most
// How often a waiting request looks whether its game gave it up.
constexpr float kLookSeconds = 0.05f;

class PlainStream : public HttpStream {
 public:
  using HttpStream::HttpStream;

  Flow send(std::vector<std::uint8_t>& bytes) override {
    const std::size_t before = bytes.size();
    if (socket_.write(bytes) == IoStatus::Closed) return Flow::Failed;
    return bytes.size() < before ? Flow::Moved : Flow::WantWrite;
  }

  Flow receive(std::vector<std::uint8_t>& into) override {
    const IoStatus status = socket_.read(into, kReadBytes);
    // (An end that came with the last bytes is seen at the next call.)
    if (!into.empty()) return Flow::Moved;
    return status == IoStatus::Closed ? Flow::Ended : Flow::WantRead;
  }

  std::string error() const override { return "the connection broke"; }
};

}  // namespace

std::unique_ptr<HttpStream> plain_stream(Socket socket) { return std::make_unique<PlainStream>(std::move(socket)); }

Patience::Patience(float seconds, const std::atomic<bool>& cancelled)
    : deadline_(std::chrono::steady_clock::now() +
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(seconds))),
      seconds_(seconds),
      cancelled_(cancelled) {}

bool Patience::over() const { return cancelled_.load() || std::chrono::steady_clock::now() >= deadline_; }

bool Patience::wait(const Socket& socket, bool sending) const {
  if (over()) return false;
  const float left = std::chrono::duration<float>(deadline_ - std::chrono::steady_clock::now()).count();
  Socket::wait({{&socket, sending}}, std::min(left, kLookSeconds));
  return !over();
}

std::string Patience::why() const {
  if (cancelled_.load()) return "given up: the game ended";
  std::ostringstream text;
  text << "no answer within " << seconds_ << " seconds";
  return text.str();
}

}  // namespace redgem::net
