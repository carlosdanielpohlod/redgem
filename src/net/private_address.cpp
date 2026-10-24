// Which addresses are of a network of this machine's own rather than of the internet.

#include "net/private_address.hpp"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <vector>

#include "net/decimal_text.hpp"

namespace redgem::net {

namespace {

// An IPv4 address: four numbers.
constexpr std::size_t kOctets = 4;
constexpr std::size_t kMostOctetDigits = 3;
constexpr int kLargestOctet = 255;
// The private ranges, by their first numbers.
constexpr int kLoopback = 127;  // 127.x: this machine
constexpr int kPrivateA = 10;   // 10.x
constexpr int kPrivateB = 172;  // 172.16.x to 172.31.x
constexpr int kPrivateBFirst = 16;
constexpr int kPrivateBLast = 31;
constexpr int kPrivateC = 192;  // 192.168.x
constexpr int kPrivateCSecond = 168;
constexpr int kLinkLocal = 169;  // 169.254.x: what a network with no server gives itself
constexpr int kLinkLocalSecond = 254;
// Where clouds tell a machine about itself, its keys among it, over plain HTTP: Amazon, Google,
// Azure and others (169.254.169.254, and Amazon's over IPv6), Alibaba (100.100.100.200).
constexpr const char* kCloudMetadataAddresses[] = {"169.254.169.254", "fd00:ec2::254", "100.100.100.200"};

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

bool between(int number, int first, int last) { return number >= first && number <= last; }

bool in_private_b(int first, int second) {
  return first == kPrivateB && between(second, kPrivateBFirst, kPrivateBLast);
}
bool in_private_c(int first, int second) { return first == kPrivateC && second == kPrivateCSecond; }
bool in_link_local(int first, int second) { return first == kLinkLocal && second == kLinkLocalSecond; }
// The ranges known by their first number alone.
bool in_whole_range(int first) { return first == kLoopback || first == kPrivateA; }

bool in_private_range(const std::vector<int>& address) {
  const int first = address[0];
  const int second = address[1];
  if (in_whole_range(first) || in_private_b(first, second)) return true;
  return in_private_c(first, second) || in_link_local(first, second);
}

}  // namespace

bool is_private_address(const std::string& numbers) {
  const std::vector<int> address = octets(numbers);
  return address.size() == kOctets && in_private_range(address);
}

bool is_cloud_metadata_address(const std::string& numbers) {
  return std::find(std::begin(kCloudMetadataAddresses), std::end(kCloudMetadataAddresses), numbers) !=
         std::end(kCloudMetadataAddresses);
}

}  // namespace redgem::net
