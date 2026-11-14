// What arrives on a STUN port from anyone on the internet: read as a game reads a server's answer
// (the address in it), and as a relay reads a request (and answers it). An address read out of an
// answer must come back the same when it is written into one and read again.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "fuzz/fuzz_target.hpp"
#include "net/stun/stun_message.hpp"

namespace {

using redgem::net::stun::MappedAddress;
using redgem::net::stun::TransactionId;

// Where a message's transaction id is: an input long enough answers the request it names, so the
// fuzzer reaches the attributes rather than stopping at a wrong id.
constexpr std::size_t kTransactionAt = 8;

TransactionId id_in(const std::string& bytes) {
  TransactionId id{};
  if (bytes.size() >= kTransactionAt + id.size()) std::copy_n(bytes.begin() + kTransactionAt, id.size(), id.begin());
  return id;
}

// Whether an address written into an answer is read back the same.
bool reads_back(const TransactionId& id, const MappedAddress& seen_at) {
  const auto again = redgem::net::stun::mapped_address(redgem::net::stun::binding_success(id, seen_at), id);
  return again && *again == seen_at;
}

void read_as_answer(const std::string& bytes) {
  const TransactionId id = id_in(bytes);
  const auto seen_at = redgem::net::stun::mapped_address(bytes, id);
  if (!seen_at) return;
  if (!reads_back(id, *seen_at)) std::abort();
}

void read_as_request(const std::string& bytes) {
  TransactionId id{};
  if (!redgem::net::stun::is_binding_request(bytes, id)) return;
  if (redgem::net::stun::binding_request(id).size() > bytes.size()) std::abort();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string bytes(reinterpret_cast<const char*>(data), size);
  read_as_answer(bytes);
  read_as_request(bytes);
  return redgem::fuzz::kKeepInput;
}
