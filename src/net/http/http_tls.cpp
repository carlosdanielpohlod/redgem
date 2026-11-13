// A request's connection through TLS (https://), with Mbed TLS: the server's certificate is
// checked against the trusted authorities and its name before anything is sent to it.

#include <mbedtls/error.h>
#include <mbedtls/ssl.h>

#include "net/http/http_stream.hpp"
#include "net/secure/tls_basics.hpp"

namespace redgem::net {

namespace {

constexpr const char* kPersonalization = "redgem-http";  // mixed into the random generator's seed

bool wants_more(int result) { return result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE; }

// Only the server's own goodbye ends a session cleanly; the connection ending without it (0, or
// CONN_EOF) may be anyone between the two cutting it.
bool is_end(int result) { return result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY; }
bool is_cut(int result) { return result == 0 || result == MBEDTLS_ERR_SSL_CONN_EOF; }

class TlsStream : public HttpStream {
 public:
  TlsStream(Socket socket, std::shared_ptr<const TrustStore> trust)
      : HttpStream(std::move(socket)), trust_(std::move(trust)) {
    mbedtls_ssl_init(&ssl_);
    mbedtls_ssl_config_init(&config_);
  }

  ~TlsStream() override {
    mbedtls_ssl_free(&ssl_);
    mbedtls_ssl_config_free(&config_);
  }

  TlsStream(const TlsStream&) = delete;
  TlsStream& operator=(const TlsStream&) = delete;

  // Sets the session up for `host`; the handshake itself happens with the first send.
  bool start(const std::string& host) {
    last_ = random_.seed(kPersonalization);
    if (last_ != 0) return false;
    last_ = mbedtls_ssl_config_defaults(&config_, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT);
    if (last_ != 0) return false;
    mbedtls_ssl_conf_authmode(&config_, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&config_, trust_->chain(), nullptr);
    mbedtls_ssl_conf_rng(&config_, TlsRandom::generate, &random_);
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
    unsigned char record[kTlsRecordBytes];
    const int result = mbedtls_ssl_read(&ssl_, record, sizeof(record));
    if (result > 0) into.insert(into.end(), record, record + result);
    return flow_of(result);
  }

  std::string error() const override {
    if (last_ == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) return "the server's certificate is not one this machine trusts";
    return tls_error_text(last_);
  }

 private:
  Flow flow_of(int result) {
    if (result > 0) return Flow::Moved;
    if (result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) return Flow::Moved;  // nothing for us: go on
    if (wants_more(result)) return result == MBEDTLS_ERR_SSL_WANT_READ ? Flow::WantRead : Flow::WantWrite;
    if (is_end(result)) return Flow::Ended;
    if (is_cut(result)) return Flow::Cut;
    last_ = result;
    return Flow::Failed;
  }

  // What Mbed TLS sends and receives through: the socket, which never waits.
  static int send_raw(void* self, const unsigned char* bytes, std::size_t count) {
    return send_through(static_cast<TlsStream*>(self)->socket_, bytes, count);
  }

  static int receive_raw(void* self, unsigned char* bytes, std::size_t count) {
    auto* stream = static_cast<TlsStream*>(self);
    return stream->intake_.take(stream->socket_, bytes, count);
  }

  std::shared_ptr<const TrustStore> trust_;  // kept for as long as the session reads it
  mbedtls_ssl_context ssl_;
  mbedtls_ssl_config config_;
  TlsRandom random_;
  SocketIntake intake_;
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
