#include "net/secure/server_pins.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>

#include "core/ports.hpp"
#include "net/secure/fingerprint.hpp"

namespace redgem::net {

namespace {

constexpr const char* kAddressKey = "address";
constexpr const char* kPortKey = "port";
constexpr const char* kFingerprintKey = "fingerprint";
constexpr std::int64_t kAnyPort = 0;
constexpr std::int64_t kNotAPort = -1;
constexpr char kRootDot = '.';
constexpr const char* kNotAList =
    "game.json: \"servers\" is a list of {\"address\", \"port\", \"fingerprint\"}: it is left out";

bool holds_text(const nlohmann::json& entry, const char* key) { return entry.contains(key) && entry[key].is_string(); }

std::string text_at(const nlohmann::json& entry, const char* key) {
  return holds_text(entry, key) ? entry[key].get<std::string>() : std::string();
}

bool is_port(const nlohmann::json& value) {
  return value.is_number_integer() && value.get<std::int64_t>() > 0 && value.get<std::int64_t>() <= kLargestPort;
}

// The port an entry names: kAnyPort when it names none, kNotAPort when what it names is no port.
std::int64_t port_at(const nlohmann::json& entry) {
  if (!entry.contains(kPortKey)) return kAnyPort;
  return is_port(entry[kPortKey]) ? entry[kPortKey].get<std::int64_t>() : kNotAPort;
}

// Whether an entry gave all a pin needs, each as it should be. (The whole fingerprint: a host's
// short code is for a game a player hosts, never for a server.)
bool complete(const std::string& address, const std::string& digits, std::int64_t port) {
  return !address.empty() && digits.size() == kFingerprintDigits && port != kNotAPort;
}

std::optional<ServerPin> server_pin(const nlohmann::json& entry) {
  if (!entry.is_object()) return std::nullopt;
  const std::string address = text_at(entry, kAddressKey);
  const std::string digits = pin_digits(text_at(entry, kFingerprintKey)).value_or(std::string());
  const std::int64_t port = port_at(entry);
  if (!complete(address, digits, port)) return std::nullopt;
  return ServerPin{address, static_cast<std::uint16_t>(port), digits};
}

std::string unusable(std::size_t index) {
  return "game.json: servers[" + std::to_string(index) +
         "] needs an \"address\", the server's whole \"fingerprint\" (64 hexadecimal digits) and, if it names one, a "
         "\"port\" from 1 to 65535: it is left out";
}

// An address as names compare: in lower case, without the root's dot ("Play.Example.com." is
// play.example.com).
bool ends_in_root_dot(const std::string& name) { return !name.empty() && name.back() == kRootDot; }

std::string plain_address(const std::string& address) {
  std::string plain = address;
  if (ends_in_root_dot(plain)) plain.pop_back();
  std::transform(plain.begin(), plain.end(), plain.begin(),
                 [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
  return plain;
}

bool on_port(const ServerPin& pin, std::uint16_t port) { return pin.port == kAnyPort || pin.port == port; }

// Whether a pin is the one for that server (a pin without a port is the address's on every port).
bool pins(const ServerPin& pin, const std::string& address, std::uint16_t port) {
  return plain_address(pin.address) == plain_address(address) && on_port(pin, port);
}

}  // namespace

std::vector<ServerPin> read_server_pins(const nlohmann::json& servers, std::vector<std::string>& warnings) {
  std::vector<ServerPin> found;
  if (!servers.is_array()) {
    warnings.emplace_back(kNotAList);
    return found;
  }
  for (std::size_t index = 0; index < servers.size(); ++index) {
    const std::optional<ServerPin> pin = server_pin(servers[index]);
    if (pin) found.push_back(*pin);
    if (!pin) warnings.push_back(unusable(index));
  }
  return found;
}

std::string pinned_fingerprint(const std::vector<ServerPin>& pins_known, const std::string& address,
                               std::uint16_t port) {
  const auto found = std::find_if(pins_known.begin(), pins_known.end(),
                                  [&](const ServerPin& pin) { return pins(pin, address, port); });
  return found != pins_known.end() ? found->digits : std::string();
}

}  // namespace redgem::net
