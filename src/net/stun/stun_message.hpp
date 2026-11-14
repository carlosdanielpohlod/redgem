#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// The few STUN messages (RFC 8489, Session Traversal Utilities for NAT) a game needs to learn the
// address the internet sees it at: a Binding request, and the answer that names that address.
// Only IPv4, and none of STUN's authentication or fingerprints: the answer is a hint about the
// route out of this machine's network, never something to trust with more than that.
namespace redgem::net::stun {

// The bytes that tell one request (and its answer) from every other.
inline constexpr std::size_t kTransactionIdBytes = 12;
using TransactionId = std::array<std::uint8_t, kTransactionIdBytes>;

// An address and port in numbers, as the internet sees a machine (or as a STUN request came from).
struct MappedAddress {
  std::string address;
  std::uint16_t port = 0;

  bool operator==(const MappedAddress& other) const { return address == other.address && port == other.port; }
  bool operator!=(const MappedAddress& other) const { return !(*this == other); }
};

// A new transaction id, from the system's source of randomness.
TransactionId random_transaction_id();
// A Binding request: what a machine sends a STUN server to learn the address it is seen at.
std::string binding_request(const TransactionId& id);
// Whether `bytes` are a Binding request (of any length, attributes and all); its transaction id
// goes into `id` when they are.
bool is_binding_request(const std::string& bytes, TransactionId& id);
// The answer to the Binding request `id`: "you are at `seen_at`" (an XOR-MAPPED-ADDRESS). Empty
// when `seen_at` is no IPv4 address in numbers.
std::string binding_success(const TransactionId& id, const MappedAddress& seen_at);
// The address a Binding success answer to request `id` names; nothing for anything else (another
// request's answer, a malformed one, an IPv6 address).
std::optional<MappedAddress> mapped_address(const std::string& response, const TransactionId& id);

}  // namespace redgem::net::stun
