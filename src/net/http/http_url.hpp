#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace redgem::net {

// The longest address a request may go to.
inline constexpr std::size_t kMaxUrlBytes = 8 * 1024;

// Where a request goes, read from its address: https://host[:port]/path?query.
struct Url {
  bool secure = true;  // https
  std::string host;
  std::uint16_t port = 0;
  std::string target = "/";  // the path and the query, as they are sent

  // The Host header: the port only when it is not the scheme's own.
  std::string authority() const;
};

// The address read; nullopt, with `error` saying why, for one that is not http:// or https://,
// has no machine, a port that is no port, credentials in it, or anything that could not travel
// as it is (spaces, line breaks). The part after # never travels and is dropped.
std::optional<Url> parse_url(const std::string& text, std::string& error);

}  // namespace redgem::net
