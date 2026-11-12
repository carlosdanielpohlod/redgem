// TlsSession: a game connection's TLS, over a socket that never waits.

#include "net/secure/tls_session.hpp"

#include <mbedtls/net_sockets.h>

#include <algorithm>

namespace redgem::net {

namespace {

constexpr const char* kPersonalization = "redgem-game";  // mixed into the random generator's seed

bool wants_more(int result) { return result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE; }

// Whether a read's result is the other side going: its goodbye, or the connection ending without
// one (which, for messages that each say how long they are, cuts nothing short unseen).
bool is_end(int result) {
  return result == 0 || result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || result == MBEDTLS_ERR_SSL_CONN_EOF;
}

// Whether a result says nothing more can move now, with nothing wrong: try again later. (A TLS 1.3
// server's ticket for resuming is of no use here: what follows it is read next time.)
bool is_pause(int result) { return wants_more(result) || result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET; }

// Whether a read brought something, with the connection still there after it.
bool got_some(std::size_t taken, int last_result) { return taken > 0 && !is_end(last_result); }

}  // namespace

TlsSession::TlsSession() {
  mbedtls_ssl_init(&ssl_);
  mbedtls_ssl_config_init(&config_);
}

TlsSession::~TlsSession() {
  mbedtls_ssl_free(&ssl_);
  mbedtls_ssl_config_free(&config_);
}

std::unique_ptr<TlsSession> TlsSession::answering(std::shared_ptr<const TlsIdentity> identity, std::string& error) {
  std::unique_ptr<TlsSession> session(new TlsSession());
  session->identity_ = std::move(identity);
  if (session->configure(MBEDTLS_SSL_IS_SERVER) && session->set_up()) return session;
  error = session->failure_;
  return nullptr;
}

std::unique_ptr<TlsSession> TlsSession::calling(const TlsTrust& trust, std::string& error) {
  std::unique_ptr<TlsSession> session(new TlsSession());
  session->trust_ = trust;
  session->calling_ = true;
  if (session->configure(MBEDTLS_SSL_IS_CLIENT) && session->set_up()) return session;
  error = session->failure_;
  return nullptr;
}

bool TlsSession::configure(int endpoint) {
  int result = random_.seed(kPersonalization);
  if (result == 0) {
    result = mbedtls_ssl_config_defaults(&config_, endpoint, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
  }
  if (result != 0) return fail(tls_error_text(result));
  mbedtls_ssl_conf_rng(&config_, TlsRandom::generate, &random_);
  mbedtls_ssl_conf_min_tls_version(&config_, MBEDTLS_SSL_VERSION_TLS1_2);
  // No authority vouches for a game's server: the side that calls checks the certificate's
  // fingerprint itself once the handshake is over (trusted), and the side called asks for none.
  mbedtls_ssl_conf_authmode(&config_, MBEDTLS_SSL_VERIFY_NONE);
  return true;
}

bool TlsSession::set_up() {
  int result = 0;
  if (identity_) result = mbedtls_ssl_conf_own_cert(&config_, identity_->certificate(), identity_->key());
  if (result == 0) result = mbedtls_ssl_setup(&ssl_, &config_);
  if (result != 0) return fail(tls_error_text(result));
  mbedtls_ssl_set_bio(&ssl_, this, send_raw, receive_raw, nullptr);
  return true;
}

Handshake TlsSession::handshake(Socket& socket) {
  if (done_) return Handshake::Done;
  socket_ = &socket;
  const int result = mbedtls_ssl_handshake(&ssl_);
  if (wants_more(result)) return Handshake::Going;
  if (result != 0) return failed_handshake(result);
  done_ = trusted();
  return done_ ? Handshake::Done : Handshake::Failed;
}

Handshake TlsSession::failed_handshake(int result) {
  fail(tls_error_text(result));
  return Handshake::Failed;
}

bool TlsSession::trusted() {
  if (!calling_) return true;
  const mbedtls_x509_crt* certificate = mbedtls_ssl_get_peer_cert(&ssl_);
  if (certificate == nullptr) return fail("the other side showed no certificate");
  const std::string fingerprint = fingerprint_of(certificate->raw.p, certificate->raw.len);
  if (trust_.accepts(fingerprint)) return true;
  return fail("the other side is not the one this game expects: its fingerprint is " +
              readable_fingerprint(fingerprint));
}

IoStatus TlsSession::read(Socket& socket, std::vector<std::uint8_t>& into, std::size_t limit) {
  socket_ = &socket;
  std::size_t taken = 0;
  int result = MBEDTLS_ERR_SSL_WANT_READ;
  while (taken < limit) {
    result = read_record(into, limit - taken);
    if (result <= 0) break;
    taken += static_cast<std::size_t>(result);
  }
  return got_some(taken, result) ? IoStatus::Ok : status_of(result);
}

int TlsSession::read_record(std::vector<std::uint8_t>& into, std::size_t most) {
  unsigned char record[kTlsRecordBytes];
  const int result = mbedtls_ssl_read(&ssl_, record, std::min(most, sizeof(record)));
  if (result > 0) into.insert(into.end(), record, record + result);
  return result;
}

IoStatus TlsSession::write(Socket& socket, std::vector<std::uint8_t>& bytes) {
  socket_ = &socket;
  std::size_t sent = 0;
  int result = MBEDTLS_ERR_SSL_WANT_WRITE;
  while (sent < bytes.size()) {
    result = write_record(bytes.data() + sent, bytes.size() - sent);
    if (result <= 0) break;
    sent += static_cast<std::size_t>(result);
  }
  bytes.erase(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(sent));
  return bytes.empty() ? IoStatus::Ok : status_of(result);
}

int TlsSession::write_record(const std::uint8_t* bytes, std::size_t count) {
  // A record cut short is asked for again with the length it had: Mbed TLS counts it as written,
  // that long, once the rest of it went out.
  const std::size_t length = unsent_ != 0 ? unsent_ : std::min(count, kTlsRecordBytes);
  const int result = mbedtls_ssl_write(&ssl_, bytes, length);
  unsent_ = wants_more(result) ? length : 0;
  return result;
}

IoStatus TlsSession::status_of(int result) {
  if (result > 0 || is_pause(result)) return IoStatus::WouldBlock;
  if (!is_end(result)) fail(tls_error_text(result));
  return IoStatus::Closed;
}

bool TlsSession::holds_unread() const {
  return !intake_.empty() || mbedtls_ssl_get_bytes_avail(&ssl_) > 0 || mbedtls_ssl_check_pending(&ssl_) != 0;
}

bool TlsSession::fail(const std::string& why) {
  if (failure_.empty()) failure_ = why;
  return false;
}

int TlsSession::send_raw(void* self, const unsigned char* bytes, std::size_t count) {
  return send_through(*static_cast<TlsSession*>(self)->socket_, bytes, count);
}

int TlsSession::receive_raw(void* self, unsigned char* bytes, std::size_t count) {
  auto* session = static_cast<TlsSession*>(self);
  return session->intake_.take(*session->socket_, bytes, count);
}

}  // namespace redgem::net
