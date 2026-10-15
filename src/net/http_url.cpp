#include "net/http_url.hpp"

#include <algorithm>
#include <cctype>

#include "net/http_message.hpp"

namespace redgem::net {

namespace {

constexpr const char* kHttpsScheme = "https://";
constexpr const char* kHttpScheme = "http://";
constexpr std::uint16_t kHttpsPort = 443;
constexpr std::uint16_t kHttpPort = 80;
constexpr std::size_t kMostPortDigits = 5;
constexpr unsigned long kLargestPort = 65535;
constexpr char kFirstVisible = '!';  // what a request line carries as it is: visible ASCII, no space
constexpr char kLastVisible = '~';
constexpr char kPathStart = '/';
constexpr char kQueryStart = '?';
constexpr const char* kAuthorityEnds = "/?";  // what ends the machine and port: a path, or a query
constexpr char kFragmentStart = '#';
constexpr char kPortStart = ':';
constexpr char kCredentialsEnd = '@';

bool starts_with(const std::string& text, const std::string& prefix) {
  return text.size() >= prefix.size() &&
         std::equal(prefix.begin(), prefix.end(), text.begin(),
                    [](char wanted, char found) { return wanted == std::tolower(static_cast<unsigned char>(found)); });
}

bool travels_as_is(const std::string& text) {
  return std::all_of(text.begin(), text.end(), [](char c) { return c >= kFirstVisible && c <= kLastVisible; });
}

bool is_host_character(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '.' || c == '-'; }

bool is_port(unsigned long number) { return number > 0 && number <= kLargestPort; }

std::optional<std::uint16_t> port_number(const std::string& digits) {
  if (!is_decimal(digits, kMostPortDigits)) return std::nullopt;
  const unsigned long port = std::stoul(digits);
  if (!is_port(port)) return std::nullopt;
  return static_cast<std::uint16_t>(port);
}

bool is_host(const std::string& host) {
  return !host.empty() && std::all_of(host.begin(), host.end(), is_host_character);
}

// The machine and the port of an address: what comes between its scheme and its path.
bool read_authority(const std::string& authority, Url& url, std::string& error) {
  if (authority.find(kCredentialsEnd) != std::string::npos) {
    error = "an address cannot carry a name and a password: send them in a header";
    return false;
  }
  const std::size_t colon = authority.find(kPortStart);
  url.host = authority.substr(0, colon);
  std::transform(url.host.begin(), url.host.end(), url.host.begin(),
                 [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  const auto port = colon == std::string::npos ? std::optional<std::uint16_t>(url.secure ? kHttpsPort : kHttpPort)
                                               : port_number(authority.substr(colon + 1));
  if (!is_host(url.host)) error = "the address names no machine";
  if (!port) error = "the address's port is not a number from 1 to 65535";
  url.port = port.value_or(0);
  return error.empty();
}

// What an address says once its scheme is read: the machine, then the path and the query.
bool read_rest(const std::string& rest, Url& url, std::string& error) {
  const std::string without_fragment = rest.substr(0, rest.find(kFragmentStart));
  const std::size_t authority_end = without_fragment.find_first_of(kAuthorityEnds);
  if (authority_end != std::string::npos) url.target = without_fragment.substr(authority_end);
  if (url.target.front() == kQueryStart) url.target.insert(url.target.begin(), kPathStart);
  return read_authority(without_fragment.substr(0, authority_end), url, error);
}

}  // namespace

std::string Url::authority() const {
  const bool usual_port = port == (secure ? kHttpsPort : kHttpPort);
  return usual_port ? host : host + kPortStart + std::to_string(port);
}

std::optional<Url> parse_url(const std::string& text, std::string& error) {
  Url url;
  url.secure = starts_with(text, kHttpsScheme);
  const bool known_scheme = url.secure || starts_with(text, kHttpScheme);
  if (text.size() > kMaxUrlBytes) error = "the address is too long";
  if (!travels_as_is(text)) error = "an address cannot hold spaces, line breaks or other letters than ASCII";
  if (!known_scheme) error = "an address starts with https:// (or http:// on this machine or network)";
  if (!error.empty()) return std::nullopt;
  const std::size_t scheme = std::string(url.secure ? kHttpsScheme : kHttpScheme).size();
  if (!read_rest(text.substr(scheme), url, error)) return std::nullopt;
  return url;
}

}  // namespace redgem::net
