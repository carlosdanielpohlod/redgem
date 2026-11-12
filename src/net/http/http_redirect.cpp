// Following redirects: where a Location sends a request, and what the request carries there.

#include "net/http/http_redirect.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string_view>
#include <vector>

#include "net/http/http_message.hpp"

namespace redgem::net {

namespace {

constexpr int kMovedPermanently = 301;
constexpr int kFound = 302;
constexpr int kSeeOther = 303;
constexpr int kTemporaryRedirect = 307;
constexpr int kPermanentRedirect = 308;
constexpr int kRedirectStatuses[] = {kMovedPermanently, kFound, kSeeOther, kTemporaryRedirect, kPermanentRedirect};
constexpr const char* kLocationHeader = "location";
constexpr const char* kGet = "GET";
constexpr const char* kPost = "POST";
constexpr const char* kSecureScheme = "https:";
constexpr const char* kPlainScheme = "http:";
constexpr const char* kAuthorityStart = "//";
constexpr std::string_view kSchemeSymbols = "+-.";  // what a scheme may hold besides letters and digits
constexpr char kSchemeEnd = ':';
constexpr char kPathStart = '/';
constexpr char kQueryStart = '?';
constexpr char kFragmentStart = '#';
constexpr const char* kCurrentSegment = ".";
constexpr const char* kParentSegment = "..";
// What tells of a body: dropped with it when a redirect turns the request into a GET.
constexpr const char* kBodyHeaders[] = {"content-type", "content-encoding", "content-language", "content-location"};
// What tells who sends the request: for the origin it was given to only.
constexpr const char* kCredentialHeaders[] = {"authorization", "cookie", "proxy-authorization"};

bool is_redirect_status(int status) {
  return std::find(std::begin(kRedirectStatuses), std::end(kRedirectStatuses), status) != std::end(kRedirectStatuses);
}

bool starts_with(const std::string& text, const std::string& prefix) { return text.rfind(prefix, 0) == 0; }

bool is_scheme_character(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || kSchemeSymbols.find(c) != std::string_view::npos;
}

// A scheme: a letter, then letters, digits, + - or .
bool is_scheme(const std::string& scheme) {
  const bool scheme_characters = std::all_of(scheme.begin(), scheme.end(), is_scheme_character);
  return scheme_characters && !scheme.empty() && std::isalpha(static_cast<unsigned char>(scheme[0])) != 0;
}

// Whether a reference starts with a scheme of its own, up to a colon.
bool has_scheme(const std::string& reference) {
  const std::size_t colon = reference.find(kSchemeEnd);
  return colon != std::string::npos && is_scheme(reference.substr(0, colon));
}

std::string scheme_of(const Url& url) { return url.secure ? kSecureScheme : kPlainScheme; }

// The segments of a path, without its first slash: "a/b/" is a, b and an empty last one.
std::vector<std::string> segments(const std::string& path) {
  std::vector<std::string> all;
  std::size_t start = 0;
  for (std::size_t slash = path.find(kPathStart); slash != std::string::npos; slash = path.find(kPathStart, start)) {
    all.push_back(path.substr(start, slash - start));
    start = slash + 1;
  }
  all.push_back(path.substr(start));
  return all;
}

bool is_dot_segment(const std::string& segment) { return segment == kCurrentSegment || segment == kParentSegment; }

// Up a folder, never above the first.
void climb(std::vector<std::string>& kept) {
  if (!kept.empty()) kept.pop_back();
}

// One segment more of a path being read: .. goes up a folder, . stays where it is.
void walk(std::vector<std::string>& kept, const std::string& segment) {
  if (segment == kParentSegment) climb(kept);
  if (!is_dot_segment(segment)) kept.push_back(segment);
}

// A path (starting with /) with its . and .. segments taken out, as a server would read it.
std::string without_dot_segments(const std::string& path) {
  const std::vector<std::string> all = segments(path.substr(1));
  std::vector<std::string> kept;
  for (const std::string& segment : all) walk(kept, segment);
  if (is_dot_segment(all.back())) kept.emplace_back();  // ("/a/b/.." is the folder "/a/")
  std::string joined;
  for (const std::string& segment : kept) joined.append(1, kPathStart).append(segment);
  return joined.empty() ? std::string(1, kPathStart) : joined;
}

// A reference's path read against the path of the address it came from (which starts with /).
std::string merged_path(const std::string& base_path, const std::string& path) {
  if (path.empty()) return base_path;  // (a query alone keeps the path)
  if (path.front() == kPathStart) return path;
  return base_path.substr(0, base_path.rfind(kPathStart) + 1) + path;
}

// A reference with no scheme and no machine (a path, a query, or both) read against `base`, the
// path and query an address had.
std::string resolved_target(const std::string& base, const std::string& reference) {
  if (reference.empty()) return base;
  const std::size_t query = reference.find(kQueryStart);
  const std::string rest = query == std::string::npos ? std::string() : reference.substr(query);
  const std::string base_path = base.substr(0, base.find(kQueryStart));
  return without_dot_segments(merged_path(base_path, reference.substr(0, query))) + rest;
}

// A Location as an address of its own, read against the one its answer came from.
std::string absolute_location(const Url& from, const std::string& location) {
  const std::string reference = location.substr(0, location.find(kFragmentStart));
  if (has_scheme(reference)) return reference;
  if (starts_with(reference, kAuthorityStart)) return scheme_of(from) + reference;
  return scheme_of(from) + kAuthorityStart + from.authority() + resolved_target(from.target, reference);
}

bool is_downgrade(const Url& from, const Url& to) { return from.secure && !to.secure; }

bool same_origin(const Url& one, const Url& other) {
  return one.secure == other.secure && one.host == other.host && one.port == other.port;
}

bool is_moved(int status) { return status == kMovedPermanently || status == kFound; }

// Whether a redirect makes the request a GET: a 303 always, a 301 or 302 when it was a POST (what
// every browser does, and the standard allows).
bool turns_into_get(int status, const std::string& method) {
  const bool moved_post = is_moved(status) && method == kPost;
  return status == kSeeOther || moved_post;
}

template <std::size_t Count>
void remove_headers(HttpHeaders& headers, const char* const (&names)[Count]) {
  for (const char* name : names) remove_header(headers, name);
}

}  // namespace

bool is_followed_redirect(const HttpResponse& response) {
  const bool answered = response.error.empty() && is_redirect_status(response.status);
  return answered && !header_value(response.headers, kLocationHeader).empty();
}

std::string url_text(const Url& url) { return scheme_of(url) + kAuthorityStart + url.authority() + url.target; }

std::optional<Url> redirect_target(const Url& from, const std::string& location, std::string& error) {
  std::optional<Url> to = parse_url(absolute_location(from, trimmed(location)), error);
  if (to && is_downgrade(from, *to)) {
    error = "a redirect from https:// to http:// is not followed";
    return std::nullopt;
  }
  return to;
}

HttpRequest redirected(HttpRequest request, int status, const Url& from, const Url& to) {
  if (turns_into_get(status, request.method)) {
    request.method = kGet;
    request.body.clear();
    remove_headers(request.headers, kBodyHeaders);
  }
  if (!same_origin(from, to)) remove_headers(request.headers, kCredentialHeaders);
  request.url = url_text(to);
  return request;
}

bool follow_redirect(HttpRequest& request, Url& url, const HttpResponse& answer, std::string& error) {
  std::optional<Url> next = redirect_target(url, header_value(answer.headers, kLocationHeader), error);
  if (!next) {
    error = "the answer's redirect cannot be followed: " + error;
    return false;
  }
  request = redirected(std::move(request), answer.status, url, *next);
  url = std::move(*next);
  return true;
}

}  // namespace redgem::net
