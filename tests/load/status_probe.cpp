#include "load/status_probe.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "net/http/http_response_parser.hpp"

namespace redgem::load {

namespace {

constexpr const char* kLocalHost = "127.0.0.1";
constexpr const char* kRequest = "GET / HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n";
constexpr int kStatusOk = 200;
constexpr int kTimeoutSeconds = 2;  // a turn of the server's lobby is far shorter
constexpr std::size_t kReadBytes = 4096;

// A socket of its own, closed when it goes.
struct Descriptor {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  ~Descriptor() {
    if (fd >= 0) ::close(fd);
  }
};

bool connect_to(const Descriptor& socket, std::uint16_t port) {
  const timeval timeout{kTimeoutSeconds, 0};
  ::setsockopt(socket.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(socket.fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  ::inet_pton(AF_INET, kLocalHost, &address.sin_addr);
  return ::connect(socket.fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
}

// Reads the answer until the server closes the connection (or the read times out).
net::HttpResponse read_answer(const Descriptor& socket) {
  net::HttpResponseParser parser;
  char bytes[kReadBytes];
  ssize_t got = 0;
  while ((got = ::recv(socket.fd, bytes, sizeof(bytes), 0)) > 0) parser.feed(bytes, static_cast<std::size_t>(got));
  if (got == 0) parser.end_of_stream();
  return parser.take();
}

bool sent_request(const Descriptor& socket) {
  const std::string request = kRequest;
  return ::send(socket.fd, request.data(), request.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(request.size());
}

// Whether the request went out on a connection made to the status port.
bool asked_for_status(const Descriptor& socket, std::uint16_t port) {
  return socket.fd >= 0 && connect_to(socket, port) && sent_request(socket);
}

}  // namespace

std::optional<nlohmann::json> read_status(std::uint16_t port, std::string& error) {
  const Descriptor socket;
  if (!asked_for_status(socket, port)) {
    error = "cannot reach the status port";
    return std::nullopt;
  }
  const net::HttpResponse answer = read_answer(socket);
  if (answer.status != kStatusOk) {
    error = "the status port answered " + std::to_string(answer.status) + " " + answer.error;
    return std::nullopt;
  }
  auto document = nlohmann::json::parse(answer.body, nullptr, false);
  if (document.is_discarded()) error = "the status is not JSON";
  return document.is_discarded() ? std::nullopt : std::optional<nlohmann::json>(document);
}

}  // namespace redgem::load
