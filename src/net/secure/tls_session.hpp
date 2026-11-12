#pragma once

#include <mbedtls/ssl.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "net/secure/fingerprint.hpp"
#include "net/secure/tls_basics.hpp"
#include "net/secure/tls_identity.hpp"
#include "net/socket/socket.hpp"

namespace redgem::net {

// How far a TLS handshake got.
enum class Handshake {
  Going,  // waiting for the other side
  Done,   // the connection is encrypted, and the other side is who it should be
  Failed  // it never will be (see TlsSession::failure)
};

// One end of an encrypted game connection: TLS 1.2 at least, 1.3 when both ends have it, never
// renegotiated (the engine's Mbed TLS is built without it). Nothing here waits: every call does
// what the socket allows now. The socket is the connection's (which moves, see Connection): each
// call is given it.
// The side that calls believes the other once the handshake is over and its certificate is the
// one its TlsTrust expects; nothing of the game is sent before that.
class TlsSession {
 public:
  // The side that was called, proving itself with `identity`. Null, with `error`, when TLS cannot start.
  static std::unique_ptr<TlsSession> answering(std::shared_ptr<const TlsIdentity> identity, std::string& error);
  // The side that calls, checking the other as `trust` says (which is encrypted).
  static std::unique_ptr<TlsSession> calling(const TlsTrust& trust, std::string& error);

  ~TlsSession();
  TlsSession(const TlsSession&) = delete;
  TlsSession& operator=(const TlsSession&) = delete;

  // Takes the handshake as far as what arrived lets it.
  Handshake handshake(Socket& socket);
  bool done() const { return done_; }
  // Appends what arrived, decrypted, to `into`: about `limit` bytes at most (a record more).
  IoStatus read(Socket& socket, std::vector<std::uint8_t>& into, std::size_t limit);
  // Sends as much of `bytes` as goes out now, encrypted, and takes it off their front.
  IoStatus write(Socket& socket, std::vector<std::uint8_t>& bytes);
  // Whether something that arrived waits in here for the next read, with nothing more to arrive.
  bool holds_unread() const;
  // Why the connection failed (its handshake, or what arrived after); empty when it did not.
  const std::string& failure() const { return failure_; }

 private:
  TlsSession();
  bool configure(int endpoint);
  bool set_up();
  // Whether the side that answered is the one it should be (see TlsTrust).
  bool trusted();
  Handshake failed_handshake(int result);
  int read_record(std::vector<std::uint8_t>& into, std::size_t most);
  int write_record(const std::uint8_t* bytes, std::size_t count);
  // What a read's or a write's last result means for the connection.
  IoStatus status_of(int result);
  bool fail(const std::string& why);

  static int send_raw(void* self, const unsigned char* bytes, std::size_t count);
  static int receive_raw(void* self, unsigned char* bytes, std::size_t count);

  mbedtls_ssl_context ssl_;
  mbedtls_ssl_config config_;
  TlsRandom random_;
  SocketIntake intake_;
  std::shared_ptr<const TlsIdentity> identity_;  // the answering side's, kept while the session uses it
  TlsTrust trust_ = TlsTrust::unverified();
  bool calling_ = false;
  Socket* socket_ = nullptr;  // the one the call at hand was given
  std::size_t unsent_ = 0;    // the length of a record Mbed TLS has not sent all of (it is asked again with it)
  bool done_ = false;
  std::string failure_;
};

}  // namespace redgem::net
