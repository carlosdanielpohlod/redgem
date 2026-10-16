// What a request looks like on its way out, and what makes one that cannot go.

#include "net/http/http_message.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>

namespace redgem::net {

namespace {

constexpr const char* kMethods[] = {"GET", "POST", "PUT", "PATCH", "DELETE"};
// The methods that always say how long their body is, even an empty one.
constexpr const char* kMethodsWithBody[] = {"POST", "PUT", "PATCH"};
// What the engine says itself about every request: a game cannot set these.
constexpr const char* kReservedHeaders[] = {"host", "content-length", "connection", "transfer-encoding"};
constexpr const char* kUserAgentHeader = "user-agent";
constexpr const char* kTokenSymbols = "!#$%&'*+-.^_`|~";  // what a header's name may hold besides letters and digits
constexpr const char* kLineBreaks = "\r\n";
constexpr const char* kLineEnd = "\r\n";
constexpr const char* kBlanks = " \t";
constexpr const char* kVersion = " HTTP/1.1";
constexpr const char* kUserAgent = "RedGem";

template <std::size_t Count>
bool listed(const char* const (&list)[Count], const std::string& word) {
  return std::any_of(std::begin(list), std::end(list), [&word](const char* entry) { return word == entry; });
}

bool is_token_character(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || std::strchr(kTokenSymbols, c) != nullptr;
}

bool is_header_name(const std::string& name) {
  return !name.empty() && std::all_of(name.begin(), name.end(), is_token_character);
}

// One line, of a length a server takes (and no NUL, which ends text for some of them).
bool is_header_value(const std::string& value) {
  const bool one_line = value.find_first_of(kLineBreaks) == std::string::npos && value.find('\0') == std::string::npos;
  return one_line && value.size() <= kMaxHttpHeaderValueBytes;
}

std::string header_problem(const std::string& name, const std::string& value) {
  if (!is_header_name(name)) return "a header's name is one word of letters, digits and - (no spaces)";
  if (listed(kReservedHeaders, lower_case(name))) return "the header " + name + " is the engine's to set";
  if (!is_header_value(value)) {
    return "the header " + name + " is longer than " + std::to_string(kMaxHttpHeaderValueBytes) +
           " bytes, or holds a line break";
  }
  return {};
}

std::string headers_problem(const HttpHeaders& headers) {
  for (const auto& [name, value] : headers) {
    std::string problem = header_problem(name, value);
    if (!problem.empty()) return problem;
  }
  return {};
}

bool is_timeout(float seconds) { return seconds > 0.0f && seconds <= kLongestHttpTimeoutSeconds; }

bool has_header(const HttpHeaders& headers, const std::string& lower_name) {
  return std::any_of(headers.begin(), headers.end(),
                     [&lower_name](const auto& header) { return lower_case(header.first) == lower_name; });
}

void add_header(std::string& text, const std::string& name, const std::string& value) {
  text.append(name).append(": ").append(value).append(kLineEnd);
}

bool says_body_length(const HttpRequest& request) {
  return !request.body.empty() || listed(kMethodsWithBody, request.method);
}

}  // namespace

std::string lower_case(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  return text;
}

std::string trimmed(const std::string& text) {
  const std::size_t first = text.find_first_not_of(kBlanks);
  if (first == std::string::npos) return {};
  return text.substr(first, text.find_last_not_of(kBlanks) - first + 1);
}

bool is_decimal(const std::string& text, std::size_t most_digits) {
  const bool digits_only =
      std::all_of(text.begin(), text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
  return digits_only && !text.empty() && text.size() <= most_digits;
}

std::string request_problem(const HttpRequest& request) {
  if (!listed(kMethods, request.method)) return "HTTP sends GET, POST, PUT, PATCH and DELETE";
  if (request.headers.size() > kMaxHttpRequestHeaders) {
    return "a request carries at most " + std::to_string(kMaxHttpRequestHeaders) + " headers";
  }
  if (request.body.size() > kMaxHttpBodyBytes) {
    return "a request's body is at most " + std::to_string(kMaxHttpBodyBytes) + " bytes";
  }
  if (!is_timeout(request.timeout_seconds)) {
    return "a timeout is more than 0 seconds and at most " +
           std::to_string(static_cast<int>(kLongestHttpTimeoutSeconds));
  }
  return headers_problem(request.headers);
}

std::string request_text(const HttpRequest& request, const Url& url) {
  std::string text = request.method + ' ' + url.target + kVersion + kLineEnd;
  add_header(text, "Host", url.authority());
  add_header(text, "Connection", "close");
  if (!has_header(request.headers, kUserAgentHeader)) add_header(text, "User-Agent", kUserAgent);
  if (says_body_length(request)) add_header(text, "Content-Length", std::to_string(request.body.size()));
  for (const auto& [name, value] : request.headers) add_header(text, name, value);
  return text.append(kLineEnd).append(request.body);
}

}  // namespace redgem::net
