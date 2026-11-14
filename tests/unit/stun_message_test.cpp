#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <random>
#include <string>

#include "net/stun/stun_message.hpp"

using namespace redgem::net::stun;

namespace {

// RFC 5769, section 2.2: a server's answer naming 192.0.2.1 port 32853, with a SOFTWARE attribute
// before the address and MESSAGE-INTEGRITY and FINGERPRINT after it.
constexpr std::uint8_t kRfcAnswer[] = {
    0x01, 0x01, 0x00, 0x3c, 0x21, 0x12, 0xa4, 0x42, 0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34, 0xd6, 0x86,
    0xfa, 0x87, 0xdf, 0xae, 0x80, 0x22, 0x00, 0x0b, 0x74, 0x65, 0x73, 0x74, 0x20, 0x76, 0x65, 0x63,
    0x74, 0x6f, 0x72, 0x20, 0x00, 0x20, 0x00, 0x08, 0x00, 0x01, 0xa1, 0x47, 0xe1, 0x12, 0xa6, 0x43,
    0x00, 0x08, 0x00, 0x14, 0x2b, 0x91, 0xf5, 0x99, 0xfd, 0x9e, 0x90, 0xc3, 0x8c, 0x74, 0x89, 0xf9,
    0x2a, 0xf9, 0xba, 0x53, 0xf0, 0x6b, 0xe7, 0xd7, 0x80, 0x28, 0x00, 0x04, 0xc0, 0x7d, 0x4c, 0x96};
constexpr TransactionId kRfcId = {0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34, 0xd6, 0x86, 0xfa, 0x87, 0xdf, 0xae};
constexpr const char* kRfcAddress = "192.0.2.1";
constexpr std::uint16_t kRfcPort = 32853;

// An answer with the plain MAPPED-ADDRESS an old server sends: 198.51.100.2 port 40000.
constexpr std::uint8_t kPlainAnswer[] = {0x01, 0x01, 0x00, 0x0c, 0x21, 0x12, 0xa4, 0x42, 0x01, 0x02, 0x03,
                                         0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x00, 0x01,
                                         0x00, 0x08, 0x00, 0x01, 0x9c, 0x40, 0xc6, 0x33, 0x64, 0x02};
constexpr TransactionId kPlainId = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};
constexpr const char* kPlainAddress = "198.51.100.2";
constexpr std::uint16_t kPlainPort = 40000;

// An answer whose XOR-ed address is IPv6 (::1 port 40000) with the plain IPv4 one after it.
constexpr std::uint8_t kIpv6ThenPlainAnswer[] = {
    0x01, 0x01, 0x00, 0x24, 0x21, 0x12, 0xa4, 0x42, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x00, 0x20, 0x00, 0x14, 0x00, 0x02, 0xbd, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x9c, 0x40, 0xc6, 0x33, 0x64, 0x02};

constexpr std::size_t kHeaderBytes = 20;
constexpr std::size_t kTypeAt = 0;
constexpr std::size_t kLengthAt = 3;  // the low byte of the length
constexpr std::size_t kCookieAt = 4;
constexpr std::size_t kFirstIdAt = 8;
constexpr int kLargestByte = 255;
constexpr std::size_t kFamilyAt = 25;  // in kPlainAnswer: the address attribute's family
constexpr char kIpv6Family = 0x02;
constexpr char kFlipAll = static_cast<char>(0xFF);
constexpr const char* kPublicAddress = "203.0.113.7";
constexpr std::uint16_t kPublicPort = 54321;
constexpr std::uint16_t kHighestPort = 65535;
constexpr const char* kNoAddress = "0.0.0.0";
constexpr const char* kEveryAddress = "255.255.255.255";
// What is not an IPv4 address in numbers.
constexpr const char* kNotAddresses[] = {"",          "relay.example.com", "1.2.3",  "1.2.3.4.5",
                                         "1.2.3.256", "1.2.3.4.",          ".1.2.3", "::1"};
// The rounds of made-up damage the answers below take, from a fixed seed so a failure repeats.
constexpr int kMutationRounds = 20000;
constexpr unsigned kMutationSeed = 5389;
constexpr std::size_t kMostFlipsPerRound = 4;
constexpr std::size_t kMostExtraBytes = 64;

std::string bytes_of(const std::uint8_t* data, std::size_t size) {
  return std::string(reinterpret_cast<const char*>(data), size);
}

// The transaction id in `bytes`, as much of it as is there.
TransactionId id_of(const std::string& bytes) {
  TransactionId id{};
  const std::size_t from = std::min(bytes.size(), kFirstIdAt);
  std::copy_n(bytes.begin() + from, std::min(bytes.size() - from, id.size()), id.begin());
  return id;
}

std::string rfc_answer() { return bytes_of(kRfcAnswer, sizeof(kRfcAnswer)); }
std::string plain_answer() { return bytes_of(kPlainAnswer, sizeof(kPlainAnswer)); }

// `bytes` with a few of them changed at random, cut short or made longer.
std::string damaged(std::string bytes, std::mt19937& random) {
  std::uniform_int_distribution<std::size_t> flips(1, kMostFlipsPerRound);
  std::uniform_int_distribution<int> any_byte(0, kLargestByte);
  for (std::size_t flip = flips(random); flip > 0; --flip) {
    bytes[std::uniform_int_distribution<std::size_t>(0, bytes.size() - 1)(random)] =
        static_cast<char>(any_byte(random));
  }
  const std::size_t length = std::uniform_int_distribution<std::size_t>(0, bytes.size() + kMostExtraBytes)(random);
  bytes.resize(length, static_cast<char>(any_byte(random)));
  return bytes;
}

}  // namespace

TEST_CASE("a binding request is a bare header with the cookie and the transaction id") {
  const TransactionId id = random_transaction_id();
  const std::string request = binding_request(id);
  CHECK(request.size() == kHeaderBytes);
  TransactionId read{};
  REQUIRE(is_binding_request(request, read));
  CHECK(read == id);
}

TEST_CASE("transaction ids are random") { CHECK(random_transaction_id() != random_transaction_id()); }

TEST_CASE("the RFC's test answer names the address it was written with") {
  const auto seen_at = mapped_address(rfc_answer(), kRfcId);
  REQUIRE(seen_at.has_value());
  CHECK(seen_at->address == kRfcAddress);
  CHECK(seen_at->port == kRfcPort);
}

TEST_CASE("an old server's plain address is read when no XOR-ed one is there") {
  const auto seen_at = mapped_address(plain_answer(), kPlainId);
  REQUIRE(seen_at.has_value());
  CHECK(*seen_at == MappedAddress{kPlainAddress, kPlainPort});
}

TEST_CASE("a plain IPv4 address is read when the XOR-ed one is IPv6") {
  const auto seen_at = mapped_address(bytes_of(kIpv6ThenPlainAnswer, sizeof(kIpv6ThenPlainAnswer)), kPlainId);
  REQUIRE(seen_at.has_value());
  CHECK(*seen_at == MappedAddress{kPlainAddress, kPlainPort});
}

TEST_CASE("an answer written is read back the same") {
  const TransactionId id = random_transaction_id();
  for (const MappedAddress& seen_at : {MappedAddress{kPublicAddress, kPublicPort}, MappedAddress{kNoAddress, 0},
                                       MappedAddress{kEveryAddress, kHighestPort}}) {
    const auto read = mapped_address(binding_success(id, seen_at), id);
    REQUIRE(read.has_value());
    CHECK(*read == seen_at);
  }
}

TEST_CASE("an answer to another request is not taken") {
  TransactionId other = kRfcId;
  other.back() ^= 1;
  CHECK_FALSE(mapped_address(rfc_answer(), other).has_value());
}

TEST_CASE("a request is not an answer, and an answer is not a request") {
  const TransactionId id = random_transaction_id();
  CHECK_FALSE(mapped_address(binding_request(id), id).has_value());
  TransactionId read{};
  CHECK_FALSE(is_binding_request(rfc_answer(), read));
}

TEST_CASE("a damaged header is refused") {
  for (const std::size_t at : {kTypeAt, kLengthAt, kCookieAt}) {
    std::string answer = rfc_answer();
    answer[at] = static_cast<char>(answer[at] ^ kFlipAll);
    CHECK_FALSE(mapped_address(answer, kRfcId).has_value());
  }
}

TEST_CASE("an answer cut short anywhere is refused") {
  const std::string answer = rfc_answer();
  for (std::size_t size = 0; size < answer.size(); ++size) {
    CHECK_FALSE(mapped_address(answer.substr(0, size), kRfcId).has_value());
  }
}

TEST_CASE("an IPv6 address is not read") {
  std::string answer = plain_answer();
  answer[kFamilyAt] = kIpv6Family;
  CHECK_FALSE(mapped_address(answer, kPlainId).has_value());
}

TEST_CASE("an answer is not written for an address that is not IPv4 in numbers") {
  const TransactionId id = random_transaction_id();
  for (const char* address : kNotAddresses) {
    CHECK(binding_success(id, MappedAddress{address, kPublicPort}).empty());
  }
}

TEST_CASE("damaged answers never crash the reader, and what it reads writes back the same") {
  std::mt19937 random(kMutationSeed);
  const std::string answers[] = {rfc_answer(), plain_answer()};
  for (int round = 0; round < kMutationRounds; ++round) {
    const std::string answer = damaged(answers[static_cast<std::size_t>(round) % std::size(answers)], random);
    const TransactionId id = id_of(answer);
    const auto seen_at = mapped_address(answer, id);
    if (!seen_at) continue;
    CHECK(mapped_address(binding_success(id, *seen_at), id) == seen_at);
  }
}
