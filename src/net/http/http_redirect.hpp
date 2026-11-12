#pragma once

#include <optional>
#include <string>

#include "net/http/http_request.hpp"
#include "net/http/http_url.hpp"

// Following an answer that sends a request elsewhere (301, 302, 303, 307 and 308, with Location).
namespace redgem::net {

// How many redirects one request follows: the answer after one more is an error.
inline constexpr int kMaxHttpRedirects = 10;

// Whether an answer sends its request to the address in its Location header: one of the redirect
// statuses, with that header, and no error.
bool is_followed_redirect(const HttpResponse& response);

// The address `url` as text: its scheme, its machine (and port when not the scheme's own) and its
// path with its query.
std::string url_text(const Url& url);

// Where a redirect's Location sends a request made to `from`: an address of its own, one with no
// scheme (//machine/path), or a path or a query read against `from`, with what cannot travel as
// it is (spaces, letters beyond ASCII) percent-encoded. nullopt, with `error`, when
// it is not an address a request may go to (as parse_url reads it), or it would take an https://
// request to http://.
std::optional<Url> redirect_target(const Url& from, const std::string& location, std::string& error);

// The request as it goes on to `to` after an answer of `status` from `from`: a 303, and a 301 or
// 302 to a POST, turn it into a GET without its body (nor the headers that told of it); and when
// `to` is another origin (scheme, machine or port), it loses its credentials (Authorization,
// Cookie, Proxy-Authorization).
HttpRequest redirected(HttpRequest request, int status, const Url& from, const Url& to);

// Follows the redirect `answer` gave to `request`, sent to `url`: both become what goes next.
// False, with `error`, when its address cannot be followed.
bool follow_redirect(HttpRequest& request, Url& url, const HttpResponse& answer, std::string& error);

}  // namespace redgem::net
