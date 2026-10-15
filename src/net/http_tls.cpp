// A request's connection through TLS (https://), with Mbed TLS: the server's certificate is
// checked against the trusted authorities and its name before anything is sent to it.

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <psa/crypto.h>

#include <algorithm>

#include "net/http_stream.hpp"

namespace redgem::net {

namespace {

constexpr std::size_t kRecordBytes = 16 * 1024;  // what one read of decrypted bytes takes at most
constexpr std::size_t kErrorTextBytes = 256;
constexpr const char* kPersonalization = "redgem-http";  // mixed into the random generator's seed

// TLS 1.3 runs on the PSA crypto, which is started once for the whole program.
bool crypto_started() {
  static const bool started = psa_crypto_init() == PSA_SUCCESS;
  return started;
}

bool wants_more(int result) { return result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE; }

bool is_end(int result) {
  return result == 0 || result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || result == MBEDTLS_ERR_SSL_CONN_EOF;
}

class TlsStream : public HttpStream {
 public:
  TlsStream(Socket socket, std::shared_ptr<const TrustStore> trust)
      : HttpStream(std::move(socket)), trust_(std::move(trust)) {
    mbedtls_ssl_init(&ssl_);
    mbedtls_ssl_config_init(&config_);
    mbedtls_ctr_drbg_init(&random_);
    mbedtls_entropy_init(&entropy_);
  }

  ~TlsStream() override {
    mbedtls_ssl_free(&ssl_);
    mbedtls_ssl_config_free(&config_);
    mbedtls_ctr_drbg_free(&random_);
    mbedtls_entropy_free(&entropy_);
  }

  TlsStream(const TlsStream&) = delete;
  TlsStream& operator=(const TlsStream&) = delete;

  // Sets the session up for `host`; the handshake itself happens with the first send.
  bool start(const std::string& host) {
    if (!crypto_started()) return false;
    last_ = seed();
    if (last_ != 0) return false;
    mbedtls_ssl_config_defaults(&config_, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&config_, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&config_, trust_->chain(), nullptr);
    mbedtls_ssl_conf_rng(&config_, mbedtls_ctr_drbg_random, &random_);
    last_ = mbedtls_ssl_setup(&ssl_, &config_);
    if (last_ == 0) last_ = mbedtls_ssl_set_hostname(&ssl_, host.c_str());
    mbedtls_ssl_set_bio(&ssl_, this, send_raw, receive_raw, nullptr);
    return last_ == 0;
  }

  Flow send(std::vector<std::uint8_t>& bytes) override {
    const int result = mbedtls_ssl_write(&ssl_, bytes.data(), bytes.size());
    if (result > 0) bytes.erase(bytes.begin(), bytes.begin() + result);
    return flow_of(result);
  }

  Flow receive(std::vector<std::uint8_t>& into) override {
    unsigned char record[kRecordBytes];
    const int result = mbedtls_ssl_read(&ssl_, record, sizeof(record));
    if (result > 0) into.insert(into.end(), record, record + result);
    return flow_of(result);
  }

  std::string error() const override {
    if (last_ == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) return "the server's certificate is not one this machine trusts";
    char text[kErrorTextBytes] = {};
    mbedtls_strerror(last_, text, sizeof(text));
    return std::string("TLS: ") + text;
  }

 private:
  int seed() {
    const auto* personal = reinterpret_cast<const unsigned char*>(kPersonalization);
    return mbedtls_ctr_drbg_seed(&random_, mbedtls_entropy_func, &entropy_, personal,
                                 std::char_traits<char>::length(kPersonalization));
  }

  Flow flow_of(int result) {
    if (result > 0) return Flow::Moved;
    if (result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) return Flow::Moved;  // nothing for us: go on
    if (wants_more(result)) return result == MBEDTLS_ERR_SSL_WANT_READ ? Flow::WantRead : Flow::WantWrite;
    if (is_end(result)) return Flow::Ended;
    last_ = result;
    return Flow::Failed;
  }

  // What Mbed TLS sends and receives through: the socket, which never waits.
  static int send_raw(void* self, const unsigned char* bytes, std::size_t count) {
    std::vector<std::uint8_t> out(bytes, bytes + count);
    const IoStatus status = static_cast<TlsStream*>(self)->socket_.write(out);
    if (out.size() < count) return static_cast<int>(count - out.size());
    return status == IoStatus::Closed ? MBEDTLS_ERR_NET_SEND_FAILED : MBEDTLS_ERR_SSL_WANT_WRITE;
  }

  static int receive_raw(void* self, unsigned char* bytes, std::size_t count) {
    return static_cast<TlsStream*>(self)->take_raw(bytes, count);
  }

  int take_raw(unsigned char* bytes, std::size_t count) {
    // (The socket reads in pieces of its own size: what Mbed TLS did not ask for yet waits here.)
    const IoStatus status = raw_in_.empty() ? socket_.read(raw_in_, count) : IoStatus::Ok;
    if (raw_in_.empty()) return status == IoStatus::Closed ? 0 : MBEDTLS_ERR_SSL_WANT_READ;
    const std::size_t taken = std::min(count, raw_in_.size());
    std::copy_n(raw_in_.begin(), taken, bytes);
    raw_in_.erase(raw_in_.begin(), raw_in_.begin() + static_cast<std::ptrdiff_t>(taken));
    return static_cast<int>(taken);
  }

  std::shared_ptr<const TrustStore> trust_;  // kept for as long as the session reads it
  mbedtls_ssl_context ssl_;
  mbedtls_ssl_config config_;
  mbedtls_ctr_drbg_context random_;
  mbedtls_entropy_context entropy_;
  std::vector<std::uint8_t> raw_in_;
  int last_ = 0;  // the last error Mbed TLS gave
};

}  // namespace

std::unique_ptr<HttpStream> secure_stream(Socket socket, const std::string& host,
                                          std::shared_ptr<const TrustStore> trust, std::string& error) {
  if (trust->empty()) {
    error = "no trusted certificates were found on this machine: https:// cannot be checked";
    return nullptr;
  }
  auto stream = std::make_unique<TlsStream>(std::move(socket), std::move(trust));
  if (stream->start(host)) return stream;
  error = stream->error();
  return nullptr;
}

}  // namespace redgem::net
