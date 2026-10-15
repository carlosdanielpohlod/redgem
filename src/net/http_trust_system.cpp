// TrustStore: where each system keeps the authorities it trusts. Nothing is bundled with the
// engine: a bundle would grow old in every game built with it, while the system's own is kept up
// to date (on Android too: its apps' own trust store is these files, which any app can read).

#include <mbedtls/x509_crt.h>

#include "net/http_trust.hpp"

#ifdef _WIN32
#include <windows.h>
// (After windows.h, which it builds on.)
#include <wincrypt.h>
#endif

namespace redgem::net {

namespace {

#ifdef _WIN32
// Windows keeps them in its certificate store called ROOT.
void add_windows_store(mbedtls_x509_crt* chain) {
  HCERTSTORE store = CertOpenSystemStoreW(0, L"ROOT");
  if (store == nullptr) return;
  for (PCCERT_CONTEXT cert = CertEnumCertificatesInStore(store, nullptr); cert != nullptr;
       cert = CertEnumCertificatesInStore(store, cert)) {
    mbedtls_x509_crt_parse_der(chain, cert->pbCertEncoded, cert->cbCertEncoded);
  }
  CertCloseStore(store, 0);
}
#else
// A file with every authority in it, or a folder of one file for each.
struct Place {
  const char* path;
  bool folder;
};

// Where to look, in order: the first place that has any is the one used.
constexpr Place kPlaces[] = {
#if defined(__ANDROID__)
    {"/apex/com.android.conscrypt/cacerts", true},  // Android 14 and newer, updated apart from the system
    {"/system/etc/security/cacerts", true},
#elif defined(__APPLE__)
    {"/etc/ssl/cert.pem", false},  // the Mac's roots, updated with it
#else
    {"/etc/ssl/certs/ca-certificates.crt", false},                 // Debian, Ubuntu, Arch, Gentoo
    {"/etc/pki/tls/certs/ca-bundle.crt", false},                   // Fedora, Red Hat
    {"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", false},  // newer Red Hat
    {"/etc/ssl/ca-bundle.pem", false},                             // openSUSE
    {"/etc/ssl/cert.pem", false},                                  // Alpine
    {"/etc/ssl/certs", true},
#endif
};

void add_place(mbedtls_x509_crt* chain, const Place& place) {
  // (Some entries of a system's list may be of a kind Mbed TLS does not read: the rest are kept.)
  if (place.folder) {
    mbedtls_x509_crt_parse_path(chain, place.path);
  } else {
    mbedtls_x509_crt_parse_file(chain, place.path);
  }
}
#endif

}  // namespace

void TrustStore::add_system_certificates() {
#ifdef _WIN32
  add_windows_store(chain_.get());
#else
  for (const Place& place : kPlaces) {
    add_place(chain_.get(), place);
    if (!empty()) return;
  }
#endif
}

}  // namespace redgem::net
