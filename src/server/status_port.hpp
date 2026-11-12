#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "net/socket/socket.hpp"
#include "server/status_request.hpp"

// A dedicated server's status port: a local HTTP GET, or a bare TCP connection, gets one JSON
// document saying how busy the server is (see server_status.hpp), and the connection is closed.
// Served on the lobby's turns: nothing here waits, a turn reads at most a bounded request from a
// bounded number of connections, and a connection that is slow or says too much is let go.
namespace redgem::server {

// Connections served at once; more wait to be taken until one is done.
inline constexpr std::size_t kMostStatusClients = 8;
// The longest a connection is kept, its answer included: one that does not read it is let go.
inline constexpr float kMostStatusClientSeconds = 5.0f;

class StatusPort {
 public:
  using Document = std::function<std::string()>;

  // Listens on `port` of `address` (in numbers); false with `error` when it cannot.
  bool open(const std::string& address, std::uint16_t port, std::string& error);
  bool is_open() const { return listener_.valid(); }
  // The port it got (0 when it is not open).
  std::uint16_t port() const { return listener_.local_port(); }
  // A turn of `seconds`: takes newcomers, reads what they said and answers those due an answer
  // with `document` (called only then).
  void serve(float seconds, const Document& document);
  void close();

 private:
  enum class Stage { Listening, Answering, Done };

  struct Client {
    net::Socket socket;
    std::vector<std::uint8_t> received;
    std::vector<std::uint8_t> answer;
    float age = 0.0f;
    Stage stage = Stage::Listening;
  };

  void take_newcomers();
  void serve_client(Client& client, float seconds, const Document& document);
  static void listen_to(Client& client, const Document& document);
  static void answer(Client& client, StatusReply reply, const Document& document);
  static void send_answer(Client& client);
  void drop_done();

  net::Socket listener_;
  std::vector<Client> clients_;
};

}  // namespace redgem::server
