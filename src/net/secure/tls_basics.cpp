#include "net/secure/tls_basics.hpp"

#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <psa/crypto.h>

#include <algorithm>
#include <string>

namespace redgem::net {

bool tls_crypto_started() {
  // (TLS 1.3 runs on the PSA crypto, which is started once for the whole program.)
  static const bool started = psa_crypto_init() == PSA_SUCCESS;
  return started;
}

std::string tls_error_text(int error) {
  char text[kTlsErrorTextBytes] = {};
  mbedtls_strerror(error, text, sizeof(text));
  return std::string("TLS: ") + text;
}

TlsRandom::TlsRandom() {
  mbedtls_ctr_drbg_init(&generator_);
  mbedtls_entropy_init(&entropy_);
}

TlsRandom::~TlsRandom() {
  mbedtls_ctr_drbg_free(&generator_);
  mbedtls_entropy_free(&entropy_);
}

int TlsRandom::seed(const char* personalization) {
  if (!tls_crypto_started()) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  const auto* personal = reinterpret_cast<const unsigned char*>(personalization);
  return mbedtls_ctr_drbg_seed(&generator_, mbedtls_entropy_func, &entropy_, personal,
                               std::char_traits<char>::length(personalization));
}

int TlsRandom::generate(void* self, unsigned char* bytes, std::size_t count) {
  return mbedtls_ctr_drbg_random(&static_cast<TlsRandom*>(self)->generator_, bytes, count);
}

int SocketIntake::take(Socket& socket, unsigned char* bytes, std::size_t count) {
  const IoStatus status = waiting_.empty() ? socket.read(waiting_, count) : IoStatus::Ok;
  if (waiting_.empty()) return status == IoStatus::Closed ? 0 : MBEDTLS_ERR_SSL_WANT_READ;
  const std::size_t taken = std::min(count, waiting_.size());
  std::copy_n(waiting_.begin(), taken, bytes);
  waiting_.erase(waiting_.begin(), waiting_.begin() + static_cast<std::ptrdiff_t>(taken));
  return static_cast<int>(taken);
}

int send_through(Socket& socket, const unsigned char* bytes, std::size_t count) {
  std::vector<std::uint8_t> out(bytes, bytes + count);
  const IoStatus status = socket.write(out);
  if (out.size() < count) return static_cast<int>(count - out.size());
  return status == IoStatus::Closed ? MBEDTLS_ERR_NET_SEND_FAILED : MBEDTLS_ERR_SSL_WANT_WRITE;
}

}  // namespace redgem::net
