#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// What a game asks of the web (HTTP in Ruby), and what comes back.
namespace redgem::net {

// What one game may ask of the web: an answer's size (its status line and headers apart), the
// requests on their way at once, and how long one may take unless told otherwise (and at most).
inline constexpr std::size_t kMaxHttpBodyBytes = 8 * 1024 * 1024;
inline constexpr std::size_t kMaxHttpHeadBytes = 64 * 1024;
inline constexpr std::size_t kMaxHttpRequestsInFlight = 16;
inline constexpr float kDefaultHttpTimeoutSeconds = 15.0f;
inline constexpr float kLongestHttpTimeoutSeconds = 120.0f;
// What a request may carry besides its body: headers, and how long each value may be.
inline constexpr std::size_t kMaxHttpRequestHeaders = 64;
inline constexpr std::size_t kMaxHttpHeaderValueBytes = 8 * 1024;

// Names and values, in the order they were given (an answer's names are in lower case).
using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

struct HttpRequest {
  std::string method;  // GET, POST, PUT, PATCH or DELETE
  std::string url;
  HttpHeaders headers;
  std::string body;
  float timeout_seconds = kDefaultHttpTimeoutSeconds;
};

struct HttpResponse {
  int status = 0;  // 0 when no answer came: `error` says why
  HttpHeaders headers;
  std::string body;
  std::string error;

  static HttpResponse failure(std::string why) {
    HttpResponse response;
    response.error = std::move(why);
    return response;
  }
};

// Why a request cannot go out as it is (its method, its headers, its body or its timeout); empty
// when it can. Its URL is checked apart (parse_url).
std::string request_problem(const HttpRequest& request);

}  // namespace redgem::net
