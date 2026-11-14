// STUN Binding requests and their answers, written and read byte by byte (RFC 8489, section 5).

#include "net/stun/stun_message.hpp"

#include <algorithm>
#include <random>
#include <vector>

#include "net/private_address.hpp"

namespace redgem::net::stun {

namespace {

// Every message: a type, the length of what follows the header, the magic cookie and the
// transaction id, 20 bytes in all, then attributes, each padded to four bytes.
constexpr std::uint16_t kBindingRequest = 0x0001;
constexpr std::uint16_t kBindingSuccess = 0x0101;
constexpr std::uint32_t kMagicCookie = 0x2112A442;
constexpr std::size_t kHeaderBytes = 20;
constexpr std::size_t kTypeAt = 0;
constexpr std::size_t kLengthAt = 2;
constexpr std::size_t kCookieAt = 4;
constexpr std::size_t kTransactionAt = 8;
constexpr std::size_t kAttributeHeaderBytes = 4;
constexpr std::size_t kAttributeLengthAt = 2;  // after the attribute's type
constexpr std::size_t kAttributeAlignment = 4;
// The attributes that name an address: plain (an old server's) and XOR-ed with the cookie (so that
// routers that rewrite addresses they see in packets leave it alone).
constexpr std::uint16_t kMappedAddress = 0x0001;
constexpr std::uint16_t kXorMappedAddress = 0x0020;
// An address attribute's value: a reserved byte, the family, the port, then the address.
constexpr std::size_t kFamilyAt = 1;
constexpr std::size_t kPortAt = 2;
constexpr std::size_t kAddressAt = 4;
constexpr std::uint8_t kReservedByte = 0;
constexpr std::uint8_t kFamilyIpv4 = 0x01;
constexpr std::size_t kIpv4ValueBytes = 8;
// Bytes, and the halves of 32-bit numbers.
constexpr unsigned kBitsPerByte = 8;
constexpr unsigned kBitsPerHalf = 16;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::uint32_t kHalfMask = 0xFFFF;
constexpr std::uint16_t kCookieHigh = kMagicCookie >> kBitsPerHalf;  // what a port is XOR-ed with
// An IPv4 address in numbers: four of them, dots between.
constexpr std::size_t kOctets = 4;
constexpr char kOctetSeparator = '.';
constexpr unsigned kHighestOctetShift = (kOctets - 1) * kBitsPerByte;

// One attribute of a message: its type, and where its value is.
struct Attribute {
  std::uint16_t type = 0;
  std::size_t at = 0;
  std::size_t length = 0;
};

std::uint8_t byte(const std::string& bytes, std::size_t at) { return static_cast<std::uint8_t>(bytes[at]); }

std::uint16_t read16(const std::string& bytes, std::size_t at) {
  return static_cast<std::uint16_t>((byte(bytes, at) << kBitsPerByte) | byte(bytes, at + 1));
}

std::uint32_t read32(const std::string& bytes, std::size_t at) {
  return (std::uint32_t{read16(bytes, at)} << kBitsPerHalf) | read16(bytes, at + sizeof(std::uint16_t));
}

void put16(std::string& out, std::uint16_t value) {
  out.push_back(static_cast<char>(value >> kBitsPerByte));
  out.push_back(static_cast<char>(value & kByteMask));
}

void put32(std::string& out, std::uint32_t value) {
  put16(out, static_cast<std::uint16_t>(value >> kBitsPerHalf));
  put16(out, static_cast<std::uint16_t>(value & kHalfMask));
}

std::string header(std::uint16_t type, std::size_t length, const TransactionId& id) {
  std::string out;
  put16(out, type);
  put16(out, static_cast<std::uint16_t>(length));
  put32(out, kMagicCookie);
  out.append(reinterpret_cast<const char*>(id.data()), id.size());
  return out;
}

// Whether the header's length is what follows it, in whole four-byte steps.
bool length_matches(const std::string& bytes) {
  const std::size_t after_header = bytes.size() - kHeaderBytes;
  return read16(bytes, kLengthAt) == after_header && after_header % kAttributeAlignment == 0;
}

// Whether `bytes` are a whole STUN message of that type.
bool is_message_of(const std::string& bytes, std::uint16_t type) {
  if (bytes.size() < kHeaderBytes) return false;
  return read16(bytes, kTypeAt) == type && read32(bytes, kCookieAt) == kMagicCookie && length_matches(bytes);
}

TransactionId transaction_of(const std::string& bytes) {
  TransactionId id{};
  std::copy_n(bytes.begin() + kTransactionAt, id.size(), id.begin());
  return id;
}

bool answers(const std::string& bytes, const TransactionId& id) {
  return is_message_of(bytes, kBindingSuccess) && transaction_of(bytes) == id;
}

std::size_t padded(std::size_t length) {
  return (length + kAttributeAlignment - 1) / kAttributeAlignment * kAttributeAlignment;
}

// The attribute that starts at `at`; nothing when it runs past the end of the message.
std::optional<Attribute> attribute_at(const std::string& bytes, std::size_t at) {
  if (bytes.size() - at < kAttributeHeaderBytes) return std::nullopt;
  const Attribute attribute{read16(bytes, at), at + kAttributeHeaderBytes, read16(bytes, at + kAttributeLengthAt)};
  if (bytes.size() - attribute.at < attribute.length) return std::nullopt;
  return attribute;
}

// Every attribute of a message whose header was checked; nothing when one is cut short.
std::optional<std::vector<Attribute>> attributes_of(const std::string& bytes) {
  std::vector<Attribute> found;
  for (std::size_t at = kHeaderBytes; at < bytes.size();) {
    const auto attribute = attribute_at(bytes, at);
    if (!attribute) return std::nullopt;
    found.push_back(*attribute);
    at = attribute->at + padded(attribute->length);
  }
  return found;
}

const Attribute* find_attribute(const std::vector<Attribute>& found, std::uint16_t type) {
  const auto it = std::find_if(found.begin(), found.end(), [type](const Attribute& a) { return a.type == type; });
  return it == found.end() ? nullptr : &*it;
}

std::string dotted(std::uint32_t address) {
  std::string text;
  for (unsigned shift = 0; shift <= kHighestOctetShift; shift += kBitsPerByte) {
    if (!text.empty()) text.insert(text.begin(), kOctetSeparator);
    text.insert(0, std::to_string((address >> shift) & kByteMask));
  }
  return text;
}

bool is_ipv4_value(const std::string& bytes, const Attribute& attribute) {
  return attribute.length == kIpv4ValueBytes && byte(bytes, attribute.at + kFamilyAt) == kFamilyIpv4;
}

std::optional<MappedAddress> read_ipv4(const std::string& bytes, const Attribute& attribute, bool xored) {
  if (!is_ipv4_value(bytes, attribute)) return std::nullopt;
  std::uint16_t port = read16(bytes, attribute.at + kPortAt);
  std::uint32_t address = read32(bytes, attribute.at + kAddressAt);
  if (xored) {
    port = static_cast<std::uint16_t>(port ^ kCookieHigh);
    address ^= kMagicCookie;
  }
  return MappedAddress{dotted(address), port};
}

// The IPv4 address the attribute of that type holds; nothing when there is none.
std::optional<MappedAddress> ipv4_attribute(const std::string& bytes, const std::vector<Attribute>& found,
                                            std::uint16_t type, bool xored) {
  const Attribute* attribute = find_attribute(found, type);
  if (attribute == nullptr) return std::nullopt;
  return read_ipv4(bytes, *attribute, xored);
}

}  // namespace

TransactionId random_transaction_id() {
  std::random_device system_source;
  std::uniform_int_distribution<unsigned> any_byte(0, kByteMask);
  TransactionId id{};
  for (auto& part : id) part = static_cast<std::uint8_t>(any_byte(system_source));
  return id;
}

std::string binding_request(const TransactionId& id) { return header(kBindingRequest, 0, id); }

bool is_binding_request(const std::string& bytes, TransactionId& id) {
  if (!is_message_of(bytes, kBindingRequest)) return false;
  id = transaction_of(bytes);
  return true;
}

std::string binding_success(const TransactionId& id, const MappedAddress& seen_at) {
  const auto address = ipv4_number(seen_at.address);
  if (!address) return {};
  std::string out = header(kBindingSuccess, kAttributeHeaderBytes + kIpv4ValueBytes, id);
  put16(out, kXorMappedAddress);
  put16(out, static_cast<std::uint16_t>(kIpv4ValueBytes));
  out.push_back(static_cast<char>(kReservedByte));
  out.push_back(static_cast<char>(kFamilyIpv4));
  put16(out, static_cast<std::uint16_t>(seen_at.port ^ kCookieHigh));
  put32(out, *address ^ kMagicCookie);
  return out;
}

std::optional<MappedAddress> mapped_address(const std::string& response, const TransactionId& id) {
  if (!answers(response, id)) return std::nullopt;
  const auto found = attributes_of(response);
  if (!found) return std::nullopt;
  // (An XOR-ed address that is no IPv4 one, IPv6 say, leaves the plain one to be read, if any.)
  if (const auto xored = ipv4_attribute(response, *found, kXorMappedAddress, true)) return xored;
  return ipv4_attribute(response, *found, kMappedAddress, false);
}

}  // namespace redgem::net::stun
