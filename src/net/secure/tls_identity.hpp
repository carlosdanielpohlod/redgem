#pragma once

#include <filesystem>
#include <memory>
#include <ostream>
#include <string>

struct mbedtls_x509_crt;
struct mbedtls_pk_context;

namespace redgem::net {

// The files a server keeps its identity in, in the folder it is given (the private key readable
// by its owner alone).
inline constexpr const char* kCertificateFile = "tls-certificate.pem";
inline constexpr const char* kPrivateKeyFile = "tls-private-key.pem";

// What a machine that others connect to proves itself with: a certificate, and the private key
// that goes with it. Games pin it by its fingerprint (see fingerprint.hpp), so a certificate of
// its own (self-signed) is as good as one from an authority. Never changed once made: one serves
// every connection of the program, on the thread that takes connections in.
class TlsIdentity {
 public:
  // A new one, held in memory only: a player's hosted game, or a server with nowhere to keep it.
  static std::shared_ptr<const TlsIdentity> generate(std::string& error);
  // A certificate and its private key, in PEM text (an operator's own, or one kept before).
  static std::shared_ptr<const TlsIdentity> from_pem(const std::string& certificate, const std::string& key,
                                                     std::string& error);
  // The one kept in `folder` (kCertificateFile and kPrivateKeyFile): made and written there the
  // first time, when neither file is there. Null, with `error`, when it cannot be read or kept.
  static std::shared_ptr<const TlsIdentity> kept_in(const std::filesystem::path& folder, std::string& error);

  TlsIdentity();
  ~TlsIdentity();
  TlsIdentity(const TlsIdentity&) = delete;
  TlsIdentity& operator=(const TlsIdentity&) = delete;

  // The certificate's fingerprint, in hexadecimal digits (see fingerprint.hpp).
  const std::string& fingerprint() const { return fingerprint_; }
  // As Mbed TLS takes them (which only reads them).
  mbedtls_x509_crt* certificate() const { return certificate_.get(); }
  mbedtls_pk_context* key() const { return key_.get(); }

 private:
  bool parse(const std::string& certificate, const std::string& key, std::string& error);

  std::unique_ptr<mbedtls_x509_crt> certificate_;
  std::unique_ptr<mbedtls_pk_context> key_;
  std::string fingerprint_;
};

// What a server says of its identity when it starts: the fingerprint games pin it by (digits).
std::string fingerprint_notice(const std::string& fingerprint);
// What `--fingerprint` of redgem-relay and redgem-server does: writes the fingerprint of the
// identity kept in `folder` (made there first when there is none) to `out`, alone on its line, as
// people read it. False, with the reason logged, when there is none to be had.
bool print_fingerprint(const std::filesystem::path& folder, std::ostream& out);

}  // namespace redgem::net
