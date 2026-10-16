#include "net/http/http_trust.hpp"

#include <mbedtls/x509_crt.h>

namespace redgem::net {

TrustStore::TrustStore() : chain_(std::make_unique<mbedtls_x509_crt>()) { mbedtls_x509_crt_init(chain_.get()); }

TrustStore::~TrustStore() { mbedtls_x509_crt_free(chain_.get()); }

std::shared_ptr<const TrustStore> TrustStore::system() {
  // (Read by whichever request needs it first; the others wait for it.)
  static const std::shared_ptr<const TrustStore> store = [] {
    auto loaded = std::make_shared<TrustStore>();
    loaded->add_system_certificates();
    return loaded;
  }();
  return store;
}

std::shared_ptr<const TrustStore> TrustStore::from_pem(const std::string& pem) {
  auto store = std::make_shared<TrustStore>();
  // (Mbed TLS reads PEM text with its terminating NUL.)
  mbedtls_x509_crt_parse(store->chain_.get(), reinterpret_cast<const unsigned char*>(pem.c_str()), pem.size() + 1);
  return store;
}

bool TrustStore::empty() const { return chain_->raw.len == 0; }

}  // namespace redgem::net
