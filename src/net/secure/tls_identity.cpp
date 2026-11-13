// TlsIdentity: making a certificate of one's own, reading one, and keeping it on the disk.

#include "net/secure/tls_identity.hpp"

#include <mbedtls/ecp.h>
#include <mbedtls/pk.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/x509_crt.h>

#include <fstream>
#include <system_error>
#include <vector>

#include "core/files.hpp"
#include "core/log.hpp"
#include "net/secure/fingerprint.hpp"
#include "net/secure/tls_basics.hpp"

namespace redgem::net {

namespace {

constexpr const char* kPersonalization = "redgem-identity";  // mixed into the random generator's seed
// What a certificate of the engine's own says: whose it is (nobody checks: games pin it), when it
// is good (for as long as a game may be played), and its serial number, random and positive.
constexpr mbedtls_ecp_group_id kCurve = MBEDTLS_ECP_DP_SECP256R1;
constexpr const char* kSubject = "CN=RedGem game server";
constexpr const char* kValidFrom = "20250101000000";
constexpr const char* kValidUntil = "20991231235959";
constexpr std::size_t kSerialBytes = 16;
constexpr unsigned char kPositiveSerial = 0x7F;  // the first byte's top bit cleared
// Room for a certificate or a private key in PEM text (an elliptic curve's are far shorter).
constexpr std::size_t kPemBytes = 4096;

struct PrivateKey {
  mbedtls_pk_context pk;
  PrivateKey() { mbedtls_pk_init(&pk); }
  ~PrivateKey() { mbedtls_pk_free(&pk); }
  PrivateKey(const PrivateKey&) = delete;
  PrivateKey& operator=(const PrivateKey&) = delete;
};

struct CertificateWriter {
  mbedtls_x509write_cert crt;
  CertificateWriter() { mbedtls_x509write_crt_init(&crt); }
  ~CertificateWriter() { mbedtls_x509write_crt_free(&crt); }
  CertificateWriter(const CertificateWriter&) = delete;
  CertificateWriter& operator=(const CertificateWriter&) = delete;
};

// A certificate and its private key in PEM text.
struct PemPair {
  std::string certificate;
  std::string key;
};

int new_key(PrivateKey& key, TlsRandom& random) {
  const int result = mbedtls_pk_setup(&key.pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
  if (result != 0) return result;
  return mbedtls_ecp_gen_key(kCurve, mbedtls_pk_ec(key.pk), TlsRandom::generate, &random);
}

// What the certificate says, signed by its own key.
int describe(CertificateWriter& writer, PrivateKey& key, TlsRandom& random) {
  mbedtls_x509write_crt_set_subject_key(&writer.crt, &key.pk);
  mbedtls_x509write_crt_set_issuer_key(&writer.crt, &key.pk);
  mbedtls_x509write_crt_set_md_alg(&writer.crt, MBEDTLS_MD_SHA256);
  unsigned char serial[kSerialBytes] = {};
  int result = TlsRandom::generate(&random, serial, sizeof(serial));
  serial[0] &= kPositiveSerial;
  if (result == 0) result = mbedtls_x509write_crt_set_serial_raw(&writer.crt, serial, sizeof(serial));
  if (result == 0) result = mbedtls_x509write_crt_set_subject_name(&writer.crt, kSubject);
  if (result == 0) result = mbedtls_x509write_crt_set_issuer_name(&writer.crt, kSubject);
  if (result == 0) result = mbedtls_x509write_crt_set_validity(&writer.crt, kValidFrom, kValidUntil);
  return result;
}

int write_pem(CertificateWriter& writer, PrivateKey& key, TlsRandom& random, PemPair& pair) {
  std::vector<unsigned char> buffer(kPemBytes, 0);
  int result = mbedtls_x509write_crt_pem(&writer.crt, buffer.data(), buffer.size(), TlsRandom::generate, &random);
  if (result == 0) pair.certificate = reinterpret_cast<const char*>(buffer.data());
  if (result == 0) result = mbedtls_pk_write_key_pem(&key.pk, buffer.data(), buffer.size());
  if (result == 0) pair.key = reinterpret_cast<const char*>(buffer.data());
  // (Wiped as the other buffers of Mbed TLS are; the key's PEM text lives on in `pair`, as long as
  // the program needs it, see KNOWN_LIMITATIONS.md.)
  mbedtls_platform_zeroize(buffer.data(), buffer.size());
  return result;
}

// A new private key, and a certificate of its own for it.
bool new_pem_pair(PemPair& pair, std::string& error) {
  TlsRandom random;
  PrivateKey key;
  CertificateWriter writer;
  int result = random.seed(kPersonalization);
  if (result == 0) result = new_key(key, random);
  if (result == 0) result = describe(writer, key, random);
  if (result == 0) result = write_pem(writer, key, random, pair);
  if (result != 0) error = "cannot make a TLS certificate: " + tls_error_text(result);
  return result == 0;
}

// Writes a file only its owner may read (made so before anything is in it).
bool write_private_file(const std::filesystem::path& file, const std::string& text, std::string& error) {
  std::ofstream(file).close();
  std::error_code ignored;  // (where the system has no such permissions, the file is as the folder makes it)
  std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace, ignored);
  return write_text_file(file, text, error);
}

bool write_pem_files(const std::filesystem::path& folder, const PemPair& pair, std::string& error) {
  return write_private_file(folder / kPrivateKeyFile, pair.key, error) &&
         write_text_file(folder / kCertificateFile, pair.certificate, error);
}

bool keep_new_identity(const std::filesystem::path& folder, std::string& error) {
  PemPair pair;
  if (!new_pem_pair(pair, error)) return false;
  std::error_code ignored;  // (a folder that cannot be made shows as a file that cannot be written)
  std::filesystem::create_directories(folder, ignored);
  if (write_pem_files(folder, pair, error)) return true;
  // Half an identity would keep every later start from making a new one: none is left instead.
  std::filesystem::remove(folder / kPrivateKeyFile, ignored);
  std::filesystem::remove(folder / kCertificateFile, ignored);
  return false;
}

// Whether neither file of an identity is in `folder`: a new one is made there.
bool none_kept(const std::filesystem::path& folder) {
  std::error_code ignored;
  return !std::filesystem::exists(folder / kCertificateFile, ignored) &&
         !std::filesystem::exists(folder / kPrivateKeyFile, ignored);
}

// Whether an identity is in `folder`: one kept there before, or one made there now.
bool kept_or_made(const std::filesystem::path& folder, std::string& error) {
  return !none_kept(folder) || keep_new_identity(folder, error);
}

// The identity kept in `folder` (made there first when there is none), in PEM text.
bool kept_pem_pair(const std::filesystem::path& folder, PemPair& pair, std::string& error) {
  if (!kept_or_made(folder, error)) return false;
  const auto certificate = read_text_file(folder / kCertificateFile, error);
  if (!certificate) return false;
  const auto key = read_text_file(folder / kPrivateKeyFile, error);
  if (!key) return false;
  pair = {*certificate, *key};
  return true;
}

const unsigned char* pem_bytes(const std::string& pem) { return reinterpret_cast<const unsigned char*>(pem.c_str()); }

}  // namespace

TlsIdentity::TlsIdentity()
    : certificate_(std::make_unique<mbedtls_x509_crt>()), key_(std::make_unique<mbedtls_pk_context>()) {
  mbedtls_x509_crt_init(certificate_.get());
  mbedtls_pk_init(key_.get());
}

TlsIdentity::~TlsIdentity() {
  mbedtls_x509_crt_free(certificate_.get());
  mbedtls_pk_free(key_.get());
}

std::shared_ptr<const TlsIdentity> TlsIdentity::generate(std::string& error) {
  PemPair pair;
  if (!new_pem_pair(pair, error)) return nullptr;
  return from_pem(pair.certificate, pair.key, error);
}

std::shared_ptr<const TlsIdentity> TlsIdentity::from_pem(const std::string& certificate, const std::string& key,
                                                         std::string& error) {
  auto identity = std::make_shared<TlsIdentity>();
  if (!identity->parse(certificate, key, error)) return nullptr;
  return identity;
}

bool TlsIdentity::parse(const std::string& certificate, const std::string& key, std::string& error) {
  TlsRandom random;
  // (Mbed TLS reads PEM text with its terminating NUL.)
  int result = random.seed(kPersonalization);
  if (result == 0) result = mbedtls_x509_crt_parse(certificate_.get(), pem_bytes(certificate), certificate.size() + 1);
  if (result == 0) {
    result = mbedtls_pk_parse_key(key_.get(), pem_bytes(key), key.size() + 1, nullptr, 0, TlsRandom::generate, &random);
  }
  if (result == 0) result = mbedtls_pk_check_pair(&certificate_->pk, key_.get(), TlsRandom::generate, &random);
  if (result != 0) {
    error = "cannot use the TLS certificate and its private key: " + tls_error_text(result);
    return false;
  }
  fingerprint_ = fingerprint_of(certificate_->raw.p, certificate_->raw.len);
  return true;
}

std::shared_ptr<const TlsIdentity> TlsIdentity::kept_in(const std::filesystem::path& folder, std::string& error) {
  PemPair pair;
  if (!kept_pem_pair(folder, pair, error)) return nullptr;
  return from_pem(pair.certificate, pair.key, error);
}

std::string fingerprint_notice(const std::string& fingerprint) {
  return "TLS fingerprint, which games pin this server by (fingerprint: or game.json's \"servers\"): " +
         readable_fingerprint(fingerprint);
}

bool print_fingerprint(const std::filesystem::path& folder, std::ostream& out) {
  std::string error;
  const auto identity = TlsIdentity::kept_in(folder, error);
  if (identity == nullptr) {
    log::error(error);
    return false;
  }
  out << readable_fingerprint(identity->fingerprint()) << "\n";
  return true;
}

}  // namespace redgem::net
