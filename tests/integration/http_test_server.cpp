#include "http_test_server.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace redgem::test {

namespace {

constexpr float kLookSeconds = 0.02f;  // how long the server waits at a time, before looking whether to stop
constexpr std::size_t kReadBytes = 64 * 1024;
constexpr std::size_t kPemBytes = 4096;
constexpr const char* kName = "CN=localhost";
constexpr const char* kValidFrom = "20240101000000";
constexpr const char* kValidUntil = "20990101000000";
constexpr const char* kLengthHeader = "content-length:";
constexpr const char* kHeadEnd = "\r\n\r\n";

void check(int result, const char* what) {
  if (result != 0) throw std::runtime_error(std::string("test server: ") + what + " failed: " + std::to_string(result));
}

std::string lower(std::string text) {
  for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}

// Whether a request arrived whole: its head, and as much body as it says it has.
bool whole(const std::string& request) {
  const std::size_t head_end = request.find(kHeadEnd);
  if (head_end == std::string::npos) return false;
  const std::string head = lower(request.substr(0, head_end));
  const std::size_t length_at = head.find(kLengthHeader);
  const std::size_t length =
      length_at == std::string::npos ? 0 : std::stoul(head.substr(length_at + std::strlen(kLengthHeader)));
  return request.size() >= head_end + std::strlen(kHeadEnd) + length;
}

// One connection's bytes, through TLS or as they are.
class Channel {
 public:
  Channel(net::Socket socket, mbedtls_ssl_config* tls) : socket_(std::move(socket)), secure_(tls != nullptr) {
    mbedtls_ssl_init(&ssl_);
    if (!secure_) return;
    check(mbedtls_ssl_setup(&ssl_, tls), "ssl setup");
    mbedtls_ssl_set_bio(&ssl_, this, send_raw, receive_raw, nullptr);
  }
  ~Channel() { mbedtls_ssl_free(&ssl_); }

  void wait() const { net::Socket::wait({{&socket_, false}}, kLookSeconds); }

  // Adds what arrived to `into`; false once the other side hung up.
  bool read(std::string& into) {
    if (!secure_) {
      std::vector<std::uint8_t> got;
      const net::IoStatus status = socket_.read(got, kReadBytes);
      into.append(got.begin(), got.end());
      return status != net::IoStatus::Closed;
    }
    unsigned char record[kReadBytes];
    const int result = mbedtls_ssl_read(&ssl_, record, sizeof(record));
    if (result > 0) into.append(reinterpret_cast<char*>(record), static_cast<std::size_t>(result));
    return result > 0 || result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE ||
           result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET;
  }

  // TLS's goodbye, when the connection is through TLS.
  void say_goodbye() {
    if (secure_) mbedtls_ssl_close_notify(&ssl_);
  }

  void write_all(const std::string& bytes, const std::atomic<bool>& stop) {
    std::vector<std::uint8_t> left(bytes.begin(), bytes.end());
    while (!left.empty() && !stop.load()) {
      if (!secure_) {
        if (socket_.write(left) == net::IoStatus::Closed) return;
      } else {
        const int result = mbedtls_ssl_write(&ssl_, left.data(), left.size());
        if (result > 0) left.erase(left.begin(), left.begin() + result);
        if (result < 0 && result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE) return;
      }
      if (!left.empty()) net::Socket::wait({{&socket_, true}}, kLookSeconds);
    }
  }

 private:
  static int send_raw(void* self, const unsigned char* bytes, std::size_t count) {
    std::vector<std::uint8_t> out(bytes, bytes + count);
    static_cast<Channel*>(self)->socket_.write(out);
    if (out.size() < count) return static_cast<int>(count - out.size());
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  }

  static int receive_raw(void* self, unsigned char* bytes, std::size_t count) {
    auto& channel = *static_cast<Channel*>(self);
    net::IoStatus status = net::IoStatus::Ok;
    if (channel.raw_.empty()) status = channel.socket_.read(channel.raw_, count);
    if (channel.raw_.empty()) return status == net::IoStatus::Closed ? 0 : MBEDTLS_ERR_SSL_WANT_READ;
    const std::size_t taken = std::min(count, channel.raw_.size());
    std::copy_n(channel.raw_.begin(), taken, bytes);
    channel.raw_.erase(channel.raw_.begin(), channel.raw_.begin() + static_cast<std::ptrdiff_t>(taken));
    return static_cast<int>(taken);
  }

  net::Socket socket_;
  bool secure_;
  mbedtls_ssl_context ssl_;
  std::vector<std::uint8_t> raw_;
};

}  // namespace

// A key and a certificate for "localhost", signed by themselves, made for one test.
struct HttpTestServer::Tls {
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context random;
  mbedtls_pk_context key;
  mbedtls_x509_crt certificate;
  mbedtls_ssl_config config;
  std::string pem;

  Tls() {
    psa_crypto_init();
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&random);
    mbedtls_pk_init(&key);
    mbedtls_x509_crt_init(&certificate);
    mbedtls_ssl_config_init(&config);
    check(mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy, nullptr, 0), "seeding");
    make_key();
    pem = signed_certificate();
    check(mbedtls_x509_crt_parse(&certificate, reinterpret_cast<const unsigned char*>(pem.c_str()), pem.size() + 1),
          "reading the certificate");
    configure();
  }

  ~Tls() {
    mbedtls_ssl_config_free(&config);
    mbedtls_x509_crt_free(&certificate);
    mbedtls_pk_free(&key);
    mbedtls_ctr_drbg_free(&random);
    mbedtls_entropy_free(&entropy);
  }

  void make_key() {
    check(mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)), "key setup");
    check(mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), mbedtls_ctr_drbg_random, &random),
          "making a key");
  }

  std::string signed_certificate() {
    mbedtls_x509write_cert writer;
    mbedtls_x509write_crt_init(&writer);
    unsigned char serial[] = {1};
    mbedtls_x509write_crt_set_md_alg(&writer, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&writer, &key);
    mbedtls_x509write_crt_set_issuer_key(&writer, &key);
    check(mbedtls_x509write_crt_set_subject_name(&writer, kName), "naming");
    check(mbedtls_x509write_crt_set_issuer_name(&writer, kName), "naming the issuer");
    check(mbedtls_x509write_crt_set_serial_raw(&writer, serial, sizeof(serial)), "serial");
    check(mbedtls_x509write_crt_set_validity(&writer, kValidFrom, kValidUntil), "validity");
    check(mbedtls_x509write_crt_set_basic_constraints(&writer, 1, 0), "constraints");
    unsigned char text[kPemBytes] = {};
    check(mbedtls_x509write_crt_pem(&writer, text, sizeof(text), mbedtls_ctr_drbg_random, &random), "writing");
    mbedtls_x509write_crt_free(&writer);
    return reinterpret_cast<char*>(text);
  }

  void configure() {
    check(mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT),
          "configuring");
    mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &random);
    check(mbedtls_ssl_conf_own_cert(&config, &certificate, &key), "own certificate");
  }
};

HttpTestServer::HttpTestServer(Kind kind, std::string answer) : kind_(kind), answer_(std::move(answer)) {
  if (kind_ == Kind::Secure) {
    tls_ = std::make_unique<Tls>();
    certificate_pem_ = tls_->pem;
  }
  std::string error;
  listener_ = net::Socket::listen_on(0, error);
  if (!listener_.valid()) throw std::runtime_error("test server: " + error);
  port_ = listener_.local_port();
  thread_ = std::thread([this] { serve(); });
}

HttpTestServer::~HttpTestServer() {
  stop_.store(true);
  thread_.join();
}

std::string HttpTestServer::url(const std::string& path) const {
  const std::string machine = kind_ == Kind::Secure ? "https://localhost:" : "http://127.0.0.1:";
  return machine + std::to_string(port_) + path;
}

std::string HttpTestServer::last_request() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return last_request_;
}

void HttpTestServer::serve() {
  while (!stopping()) {
    net::Socket::wait({{&listener_, false}}, kLookSeconds);
    net::Socket client;
    if (listener_.accept(client) == net::IoStatus::Ok) serve_one(std::move(client));
  }
}

void HttpTestServer::serve_one(net::Socket socket) {
  Channel channel(std::move(socket), tls_ ? &tls_->config : nullptr);
  std::string request;
  while (!stopping() && !whole(request)) {
    channel.wait();
    if (!channel.read(request)) return;
  }
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    last_request_ = request;
  }
  ++requests_;
  while (quiet_.load() && !stopping()) {
    channel.wait();
    if (!channel.read(request)) {
      ++hang_ups_;
      return;
    }
  }
  channel.write_all(answer_, stop_);
  if (goodbye_.load()) channel.say_goodbye();
}

std::string http_answer(const std::string& status, const std::string& body, const std::string& extra_headers) {
  return "HTTP/1.1 " + status + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + extra_headers + "\r\n" +
         body;
}

}  // namespace redgem::test
