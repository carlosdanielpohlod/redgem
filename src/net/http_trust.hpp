#pragma once

#include <memory>
#include <string>

struct mbedtls_x509_crt;

namespace redgem::net {

// The certificates a secure server's own is checked against before anything is sent to it.
class TrustStore {
 public:
  // The authorities the system trusts, read once, the first time a game asks for an https://
  // address: the CA bundle of a Linux distribution or of macOS, the Windows certificate store,
  // the certificates Android's own apps trust (see http_trust_system.cpp).
  static std::shared_ptr<const TrustStore> system();
  // Only the certificates in `pem` (one's own authority, a test's).
  static std::shared_ptr<const TrustStore> from_pem(const std::string& pem);

  TrustStore();
  ~TrustStore();
  TrustStore(const TrustStore&) = delete;
  TrustStore& operator=(const TrustStore&) = delete;

  // Whether it holds no certificate at all: nothing can be trusted.
  bool empty() const;
  // As Mbed TLS takes it (which only reads it: one store serves many connections at once).
  mbedtls_x509_crt* chain() const { return chain_.get(); }

 private:
  // The system's certificates, added where this system keeps them (http_trust_system.cpp).
  void add_system_certificates();

  std::unique_ptr<mbedtls_x509_crt> chain_;
};

}  // namespace redgem::net
