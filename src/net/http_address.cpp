// Which addresses a request may reach over plain http://: this machine's and private networks'.

#include <sstream>
#include <vector>

#include "net/http_message.hpp"
#include "net/http_url.hpp"

namespace redgem::net {

namespace {

// An IPv4 address: four numbers.
constexpr std::size_t kOctets = 4;
constexpr std::size_t kMostOctetDigits = 3;
constexpr int kLargestOctet = 255;
// The private ranges, by their first numbers: this machine (127.x), private networks (10.x,
// 172.16.x to 172.31.x, 192.168.x) and addresses a network gives itself (169.254.x).
constexpr int kLoopback = 127;
constexpr int kPrivateA = 10;
constexpr int kPrivateB = 172;
constexpr int kPrivateBFirst = 16;
constexpr int kPrivateBLast = 31;
constexpr int kPrivateC = 192;
constexpr int kPrivateCSecond = 168;
constexpr int kLinkLocal = 169;
constexpr int kLinkLocalSecond = 254;

bool is_octet(const std::string& part) {
  return is_decimal(part, kMostOctetDigits) && std::stoi(part) <= kLargestOctet;
}

// The numbers of an IPv4 address; empty when it is not one.
std::vector<int> octets(const std::string& numbers) {
  std::vector<int> read;
  std::istringstream parts(numbers);
  for (std::string part; std::getline(parts, part, '.');) {
    if (!is_octet(part)) return {};
    read.push_back(std::stoi(part));
  }
  return read;
}

bool in_private_range(const std::vector<int>& address) {
  const int first = address[0];
  const int second = address[1];
  const bool private_b = first == kPrivateB && second >= kPrivateBFirst && second <= kPrivateBLast;
  const bool private_c = first == kPrivateC && second == kPrivateCSecond;
  const bool link_local = first == kLinkLocal && second == kLinkLocalSecond;
  return first == kLoopback || first == kPrivateA || private_b || private_c || link_local;
}

}  // namespace

bool is_private_address(const std::string& numbers) {
  const std::vector<int> address = octets(numbers);
  return address.size() == kOctets && in_private_range(address);
}

}  // namespace redgem::net
