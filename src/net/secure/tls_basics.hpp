#pragma once

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "net/socket/socket.hpp"

// What every TLS connection of the engine shares, the games' web requests (https://) and their
// own connections (net/secure/tls_session.hpp) alike: the crypto started once, the randomness each
// connection draws from, and the way Mbed TLS reaches a socket that never waits.
namespace redgem::net {

// What a read of TLS records takes at once: the longest record there is.
inline constexpr std::size_t kTlsRecordBytes = 16 * 1024;
// How long Mbed TLS's description of an error may be.
inline constexpr std::size_t kTlsErrorTextBytes = 256;

// Whether the PSA crypto that TLS 1.3 runs on is started (once, for the whole program).
bool tls_crypto_started();
// What Mbed TLS's error number says, in words.
std::string tls_error_text(int error);

// A random generator seeded from the system's entropy, which Mbed TLS draws from. One per
// connection: none is shared between threads.
class TlsRandom {
 public:
  TlsRandom();
  ~TlsRandom();
  TlsRandom(const TlsRandom&) = delete;
  TlsRandom& operator=(const TlsRandom&) = delete;

  // Seeds it, mixing `personalization` in; also starts the crypto. Mbed TLS's error, or 0.
  int seed(const char* personalization);
  // What Mbed TLS calls for random bytes, with the generator as `self`.
  static int generate(void* self, unsigned char* bytes, std::size_t count);

 private:
  mbedtls_ctr_drbg_context generator_;
  mbedtls_entropy_context entropy_;
};

// What arrived on a socket for Mbed TLS: a socket reads in pieces of its own size, so what Mbed
// TLS did not ask for yet waits here.
class SocketIntake {
 public:
  // Gives Mbed TLS up to `count` bytes: how many, 0 when the other side hung up, or
  // MBEDTLS_ERR_SSL_WANT_READ when nothing is there yet.
  int take(Socket& socket, unsigned char* bytes, std::size_t count);
  bool empty() const { return waiting_.empty(); }

 private:
  std::vector<std::uint8_t> waiting_;
};

// Sends what Mbed TLS gives through a socket: how many bytes went, MBEDTLS_ERR_SSL_WANT_WRITE when
// none could now, or MBEDTLS_ERR_NET_SEND_FAILED when the connection is gone.
int send_through(Socket& socket, const unsigned char* bytes, std::size_t count);

}  // namespace redgem::net
